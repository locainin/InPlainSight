// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include <fcntl.h>
#include <unistd.h>

#include "headers/shards.h"

#include "../../../include/split/manifest.h"

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

// Split hide writes multiple shard images into one directory
// Each shard is independently encrypted and includes an authenticated outer header

// The split planner reuses the same overhead model as single-cover hide
// MIME length is included in that model even though split shards do not store MIME bytes today
#define PLAINSIGHT_MIME_OCTET_STREAM_LEN 24u

plainsight_error plainsight_cli_run_hide_split(const plainsight_hide_options *options) {
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
  plainsight_error passphrase_lock_result = PLAINSIGHT_ERR_INTERNAL;
  int output_is_directory = 0;
  int payload_file_descriptor = -1;
  uint64_t payload_file_size = 0u;
  size_t passphrase_length = 0u;
  size_t payload_name_length = 0u;
  uint32_t shards_written = 0u;
  plainsight_capacity_input capacity_input;
  plainsight_capacity_report capacity_report;
  plainsight_info_report info_report;
  uint64_t per_shard_capacity = 0u;
  plainsight_split_plan split_plan;
  uint8_t set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
  plainsight_kdf_params kdf_params;
  uint8_t master_key[crypto_kdf_KEYBYTES];
  uint8_t embed_seed[32];
  uint32_t shard_index = 0u;
  uint64_t remaining_payload = 0u;
  uint32_t per_shard_plain_len[PLAINSIGHT_MAX_SHARDS];
  uint64_t per_shard_cipher_len[PLAINSIGHT_MAX_SHARDS];
  size_t manifest_len = 0u;
  char template_text[128];
  char expanded_payload_path[PLAINSIGHT_MAX_PATH_BYTES];
  plainsight_split_outer_v2 outer_header;
  plainsight_cli_hide_shard_write_context write_context;

  if (options == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->cover_path == NULL || options->payload_path == NULL || options->output_dir == NULL ||
      options->passphrase_path == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->compression_mode != PLAINSIGHT_COMPRESSION_NONE) {
    return PLAINSIGHT_ERR_UNSUPPORTED;
  }

  result_code = plainsight_cli_path_is_directory(options->output_dir, &output_is_directory);
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }
  if (output_is_directory == 0) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Payload size must be known up front for split planning
  // This prevents partial sets when the payload is longer than expected
  result_code = plainsight_io_get_regular_file_size(options->payload_path, &payload_file_size);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }
  result_code = plainsight_io_expand_home_path(options->payload_path, expanded_payload_path,
                                               sizeof(expanded_payload_path));
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }
  if (payload_file_size > (uint64_t)PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES) {
    result_code = PLAINSIGHT_ERR_TOO_LARGE;
    goto cleanup;
  }

  // Payload basename is included so split planning matches the same overhead model used by single hide
  // This keeps "info required_shards" consistent with "hide split output count"
  result_code = plainsight_io_copy_basename(options->payload_path, g_cli_workspace.payload_name,
                                            sizeof(g_cli_workspace.payload_name), &payload_name_length);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Load cover once for capacity planning and seed derivation
  // The per-shard loop reloads the cover again so pixel changes do not stack across shards
  result_code = plainsight_cli_load_image(options->cover_path, &g_cli_workspace.image);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  capacity_input.cover_data_bytes = (uint64_t)g_cli_workspace.image.data_len;
  capacity_input.lsb_bits = 1u;
  capacity_input.density_per_mille = 1000u;
  capacity_input.payload_name_len = payload_name_length;
  capacity_input.mime_len = PLAINSIGHT_MIME_OCTET_STREAM_LEN;

  result_code = plainsight_capacity_compute_lsb(&capacity_input, &capacity_report);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Per-shard capacity is clamped by the global per-shard payload cap
  // The embedding layer can store more bits, but workspace buffers are fixed in size
  per_shard_capacity = capacity_report.max_payload_by_cover_bytes;
  if (per_shard_capacity > (uint64_t)PLAINSIGHT_MAX_SHARD_PLAINTEXT_BYTES) {
    per_shard_capacity = (uint64_t)PLAINSIGHT_MAX_SHARD_PLAINTEXT_BYTES;
  }
  if (per_shard_capacity == 0u) {
    result_code = PLAINSIGHT_ERR_CAPACITY;
    goto cleanup;
  }

  result_code = plainsight_split_plan_compute(payload_file_size, per_shard_capacity, &split_plan);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Default template uses a lossless extension derived from the cover format
  // Split mode writes one file per shard, so the template must include an index slot
  if (options->output_template != NULL) {
    size_t index = 0u;
    while (options->output_template[index] != '\0') {
      if (index + 1u >= sizeof(template_text)) {
        result_code = PLAINSIGHT_ERR_TOO_LARGE;
        goto cleanup;
      }
      template_text[index] = options->output_template[index];
      index++;
    }
    template_text[index] = '\0';
  } else {
    result_code = plainsight_cli_split_build_default_template(options->cover_path, template_text,
                                                              sizeof(template_text));
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }
  }

  // Validate template once and preflight shard paths so failure does not leave partial sets
  result_code =
      plainsight_cli_split_preflight_outputs(options->output_dir, template_text, split_plan.shard_count);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  result_code = plainsight_info_build_report(
      &g_cli_workspace.image, plainsight_image_detect_format_from_path(options->cover_path), 1u, 1000u,
      payload_name_length, PLAINSIGHT_MIME_OCTET_STREAM_LEN, 1, payload_file_size, &info_report);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }
  plainsight_cli_hide_shard_log_preflight(&info_report, template_text, payload_file_size);

  // Passphrase bytes are treated as raw bytes and are not NUL-terminated
  // This keeps passphrases compatible with binary UI input paths
  result_code = plainsight_io_read_passphrase_file(options->passphrase_path, g_cli_workspace.passphrase,
                                                   sizeof(g_cli_workspace.passphrase), &passphrase_length);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }
  passphrase_lock_result = plainsight_secure_lock(g_cli_workspace.passphrase, passphrase_length);

  // Derive embedding seed once from the cover bytes
  // Seed stays stable after embedding because it hashes the cover with LSB masked out
  // This allows extracting the outer header without knowing the encryption header salt
  result_code = plainsight_crypto_seed_from_passphrase_and_cover(
      g_cli_workspace.passphrase, passphrase_length, g_cli_workspace.image.pixels,
      g_cli_workspace.image.data_len, embed_seed);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // KDF params are stored in every shard header for extraction
  // This build uses the interactive defaults to keep UI latency reasonable
  kdf_params.opslimit = (uint64_t)crypto_pwhash_OPSLIMIT_INTERACTIVE;
  kdf_params.memlimit = (uint64_t)crypto_pwhash_MEMLIMIT_INTERACTIVE;
  kdf_params.alg = (uint16_t)crypto_pwhash_ALG_ARGON2ID13;

  // set_id groups shards from one hide operation
  // set_id is not secret, but it prevents mixing shards from unrelated runs
  result_code = plainsight_crypto_fill_random(set_id, sizeof(set_id));
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // One random salt is shared by all shards so extraction derives the master key once
  result_code = plainsight_crypto_fill_random(outer_header.salt, sizeof(outer_header.salt));
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Master key is derived once then expanded into per-shard keys
  // Per-shard keys prevent nonce reuse mistakes across shards
  result_code = plainsight_crypto_derive_master_key(g_cli_workspace.passphrase, passphrase_length,
                                                    outer_header.salt, &kdf_params, master_key);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Build per-shard length tables for the encrypted manifest
  // The manifest makes extraction deterministic and avoids guessing how much data lives in each shard
  remaining_payload = payload_file_size;
  for (shard_index = 0u; shard_index < split_plan.shard_count; shard_index++) {
    uint64_t take_now = 0u;
    uint64_t cipher_len_now = 0u;

    if (shard_index == 0u) {
      take_now = remaining_payload;
      if (take_now > (uint64_t)split_plan.shard0_max_plain_data_len) {
        take_now = (uint64_t)split_plan.shard0_max_plain_data_len;
      }
    } else {
      take_now = remaining_payload;
      if (take_now > per_shard_capacity) {
        take_now = per_shard_capacity;
      }
    }

    if (take_now > UINT32_MAX) {
      result_code = PLAINSIGHT_ERR_TOO_LARGE;
      goto cleanup;
    }
    per_shard_plain_len[shard_index] = (uint32_t)take_now;

    // Ciphertext length includes the AEAD tag and includes manifest bytes for shard 0
    if (shard_index == 0u) {
      cipher_len_now =
          (uint64_t)split_plan.manifest_len + take_now + (uint64_t)PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES;
    } else {
      cipher_len_now = take_now + (uint64_t)PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES;
    }
    per_shard_cipher_len[shard_index] = cipher_len_now;

    remaining_payload -= take_now;
  }
  if (remaining_payload != 0u) {
    result_code = PLAINSIGHT_ERR_INTERNAL;
    goto cleanup;
  }

  // Open payload once and read sequentially so memory stays bounded
  // open does not expand ~/ paths, so use the same real path checked above
  payload_file_descriptor = open(expanded_payload_path, O_RDONLY | O_CLOEXEC);
  if (payload_file_descriptor < 0) {
    result_code = PLAINSIGHT_ERR_IO;
    goto cleanup;
  }

  // Pack shard 0 manifest once so the exact bytes are embedded
  // This avoids recomputing the manifest inside the shard loop and accidentally changing its length
  result_code = plainsight_split_manifest_pack(
      PLAINSIGHT_SPLIT_MANIFEST_VERSION, PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN, set_id,
      payload_file_size, 0u, split_plan.chunk_plain_len, split_plan.shard_count, per_shard_plain_len,
      per_shard_cipher_len, g_cli_workspace.inner, sizeof(g_cli_workspace.inner), &manifest_len);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }
  if (manifest_len != split_plan.manifest_len) {
    result_code = PLAINSIGHT_ERR_INTERNAL;
    goto cleanup;
  }

  // Shard writing is isolated so this command stays focused on setup and cleanup
  write_context.options = options;
  write_context.split_plan = &split_plan;
  write_context.kdf_params = &kdf_params;
  write_context.set_id = set_id;
  write_context.template_text = template_text;
  write_context.payload_file_descriptor = payload_file_descriptor;
  write_context.shards_written = &shards_written;
  write_context.per_shard_plain_len = per_shard_plain_len;
  write_context.per_shard_cipher_len = per_shard_cipher_len;
  write_context.manifest_len = manifest_len;
  write_context.master_key = master_key;
  write_context.embed_seed = embed_seed;
  write_context.outer_header = &outer_header;
  result_code = plainsight_cli_hide_shard_write_outputs(&write_context);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  result_code = PLAINSIGHT_OK;

cleanup:
  if (payload_file_descriptor >= 0) {
    (void)close(payload_file_descriptor);
  }

  if (result_code != PLAINSIGHT_OK && options != NULL && options->output_dir != NULL) {
    plainsight_cli_hide_shard_cleanup_outputs(options, template_text, shards_written);
  }

  plainsight_secure_zero(embed_seed, sizeof(embed_seed));
  plainsight_secure_zero(master_key, sizeof(master_key));
  plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
  plainsight_secure_zero(g_cli_workspace.ciphertext, sizeof(g_cli_workspace.ciphertext));
  plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));
  plainsight_secure_zero(g_cli_workspace.payload_name, sizeof(g_cli_workspace.payload_name));

  if (passphrase_lock_result == PLAINSIGHT_OK) {
    (void)plainsight_secure_unlock(g_cli_workspace.passphrase, passphrase_length);
  } else {
    plainsight_secure_zero(g_cli_workspace.passphrase, passphrase_length);
  }

  return result_code;
}
