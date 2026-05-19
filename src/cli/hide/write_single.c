// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>

#include "headers/single.h"

plainsight_error plainsight_cli_run_hide(const plainsight_hide_options *options) {
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
  plainsight_error passphrase_lock_result = PLAINSIGHT_ERR_INTERNAL;
  plainsight_error payload_size_result = PLAINSIGHT_ERR_INTERNAL;
  int payload_loaded = 0;
  int output_exists = 0;
  plainsight_kdf_params kdf_params;
  plainsight_inner_header inner_header;
  plainsight_outer_header outer_header;
  uint8_t encryption_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
  uint8_t embed_seed[32];
  size_t passphrase_length = 0u;
  size_t payload_length = 0u;
  size_t inner_length = 0u;
  size_t ciphertext_length = 0u;
  size_t container_length = 0u;
  size_t payload_name_length = 0u;
  uint64_t payload_file_size = 0u;
  uint64_t max_payload_by_cover = 0u;
  uint64_t effective_payload_file_size = 0u;
  uint8_t final_compression_mode = PLAINSIGHT_COMPRESSION_NONE;
  plainsight_capacity_input capacity_input;
  plainsight_capacity_report capacity_report;
  plainsight_info_report info_report;
  static const uint8_t mime_octet_stream[] = {
      (uint8_t)'a', (uint8_t)'p', (uint8_t)'p', (uint8_t)'l', (uint8_t)'i', (uint8_t)'c',
      (uint8_t)'a', (uint8_t)'t', (uint8_t)'i', (uint8_t)'o', (uint8_t)'n', (uint8_t)'/',
      (uint8_t)'o', (uint8_t)'c', (uint8_t)'t', (uint8_t)'e', (uint8_t)'t', (uint8_t)'-',
      (uint8_t)'s', (uint8_t)'t', (uint8_t)'r', (uint8_t)'e', (uint8_t)'a', (uint8_t)'m'};

  if (options == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->split_auto != 0) {
    // Split coordinator lives in a dedicated module to keep this file focused
    return plainsight_cli_run_hide_split(options);
  }

  // Refuse overwrite so automation cannot accidentally destroy existing files
  // Single-image hide uses atomic output, but overwrite refusal is still enforced up front
  result_code = plainsight_cli_path_exists(options->output_path, &output_exists);
  if (result_code != PLAINSIGHT_OK) {
    (void)fputs("hide output preflight failed: could not inspect output path\n", stderr);
    return result_code;
  }
  if (output_exists != 0) {
    (void)fputs("hide output preflight failed: output path already exists: ", stderr);
    (void)fputs(options->output_path, stderr);
    (void)fputc('\n', stderr);
    return PLAINSIGHT_ERR_IO;
  }

  // Parse basename early so preflight can account for inner metadata overhead
  result_code = plainsight_io_copy_basename(options->payload_path, g_cli_workspace.payload_name,
                                            sizeof(g_cli_workspace.payload_name), &payload_name_length);
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }

  if (payload_name_length > (size_t)UINT16_MAX) {
    return PLAINSIGHT_ERR_TOO_LARGE;
  }
  if (sizeof(mime_octet_stream) > (size_t)UINT16_MAX) {
    return PLAINSIGHT_ERR_TOO_LARGE;
  }

  // Decode cover before payload read so capacity checks can fail early with details
  result_code = plainsight_cli_load_image(options->cover_path, &g_cli_workspace.image);
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }

  capacity_input.cover_data_bytes = (uint64_t)g_cli_workspace.image.data_len;
  capacity_input.lsb_bits = 1u;
  capacity_input.density_per_mille = 1000u;
  capacity_input.payload_name_len = payload_name_length;
  capacity_input.mime_len = sizeof(mime_octet_stream);

  result_code = plainsight_capacity_compute_lsb(&capacity_input, &capacity_report);
  if (result_code != PLAINSIGHT_OK) {
    return result_code;
  }
  max_payload_by_cover = capacity_report.max_payload_by_cover_bytes;

  payload_size_result = plainsight_io_get_regular_file_size(options->payload_path, &payload_file_size);
  if (payload_size_result == PLAINSIGHT_OK) {
    // Project cap is a hard bound independent of cover dimensions
    if (payload_file_size > (uint64_t)PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES) {
      plainsight_cli_hide_log_size_limits(payload_file_size, max_payload_by_cover);
      return PLAINSIGHT_ERR_TOO_LARGE;
    }
    effective_payload_file_size = payload_file_size;
  }

  if (options->compression_mode != PLAINSIGHT_COMPRESSION_NONE || payload_size_result != PLAINSIGHT_OK) {
    size_t compressed_length = 0u;

    // Compression decisions need real bytes, but this still happens before secrets are loaded
    result_code = plainsight_io_read_file(options->payload_path, g_cli_workspace.payload,
                                          sizeof(g_cli_workspace.payload), &payload_length);
    if (result_code != PLAINSIGHT_OK) {
      if (result_code == PLAINSIGHT_ERR_TOO_LARGE) {
        (void)fputs("hide preflight failed: payload file is larger than project cap of ", stderr);
        plainsight_cli_hide_write_u64_stderr((uint64_t)PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES);
        (void)fputs(" bytes\n", stderr);
      } else {
        (void)fputs("hide preflight failed: could not read payload path: ", stderr);
        (void)fputs(options->payload_path, stderr);
        (void)fputc('\n', stderr);
      }
      goto cleanup_without_secret;
    }
    payload_loaded = 1;
    payload_file_size = (uint64_t)payload_length;
    effective_payload_file_size = payload_file_size;

    if (options->compression_mode == PLAINSIGHT_COMPRESSION_ZSTD) {
      result_code =
          plainsight_compress_zstd(g_cli_workspace.payload, payload_length, g_cli_workspace.ciphertext,
                                   sizeof(g_cli_workspace.ciphertext), &compressed_length);
      if (result_code != PLAINSIGHT_OK) {
        goto cleanup_without_secret;
      }
      final_compression_mode = PLAINSIGHT_COMPRESSION_ZSTD;
      effective_payload_file_size = (uint64_t)compressed_length;
    } else if (options->compression_mode == PLAINSIGHT_CLI_COMPRESSION_AUTO) {
      result_code = plainsight_cli_hide_choose_auto_compression(payload_length, &compressed_length);
      if (result_code == PLAINSIGHT_OK) {
        final_compression_mode = PLAINSIGHT_COMPRESSION_ZSTD;
        effective_payload_file_size = (uint64_t)compressed_length;
      } else {
        compressed_length = 0u;
      }
    }
  }

  {
    plainsight_info_report_input report_input = {
        &g_cli_workspace.image,
        plainsight_image_detect_format_from_path(options->cover_path),
        1u,
        1000u,
        payload_name_length,
        sizeof(mime_octet_stream),
        1,
        effective_payload_file_size};
    result_code = plainsight_info_build_report(&report_input, &info_report);
  }
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup_without_secret;
  }

  {
    plainsight_cli_hide_preflight_log_input preflight_log = {payload_file_size, effective_payload_file_size,
                                                             final_compression_mode};
    plainsight_cli_hide_log_preflight(&info_report, &preflight_log);
  }

  // Cover-derived max payload reflects all container overhead in this run
  if (effective_payload_file_size > max_payload_by_cover) {
    plainsight_cli_hide_log_size_limits(effective_payload_file_size, max_payload_by_cover);
    result_code = PLAINSIGHT_ERR_CAPACITY;
    goto cleanup_without_secret;
  }

  if (payload_loaded == 0) {
    // Payload stays in memory, no plaintext temporary file is created
    result_code = plainsight_io_read_file(options->payload_path, g_cli_workspace.payload,
                                          sizeof(g_cli_workspace.payload), &payload_length);
    if (result_code != PLAINSIGHT_OK) {
      if (result_code == PLAINSIGHT_ERR_TOO_LARGE) {
        (void)fputs("hide preflight failed: payload file is larger than project cap of ", stderr);
        plainsight_cli_hide_write_u64_stderr((uint64_t)PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES);
        (void)fputs(" bytes\n", stderr);
      } else {
        (void)fputs("hide failed: could not read payload path: ", stderr);
        (void)fputs(options->payload_path, stderr);
        (void)fputc('\n', stderr);
      }
      goto cleanup_without_secret;
    }
  }

  if (final_compression_mode == PLAINSIGHT_COMPRESSION_ZSTD) {
    inner_header.payload = g_cli_workspace.ciphertext;
    inner_header.payload_len = (size_t)effective_payload_file_size;
  } else {
    inner_header.payload = g_cli_workspace.payload;
    inner_header.payload_len = payload_length;
  }

  // Secrets enter memory once and are locked when possible
  result_code = plainsight_io_read_passphrase_file(options->passphrase_path, g_cli_workspace.passphrase,
                                                   sizeof(g_cli_workspace.passphrase), &passphrase_length);
  if (result_code != PLAINSIGHT_OK) {
    (void)fputs("hide failed: could not read passphrase file: ", stderr);
    (void)fputs(options->passphrase_path, stderr);
    (void)fputc('\n', stderr);
    goto cleanup_without_secret;
  }
  passphrase_lock_result = plainsight_secure_lock(g_cli_workspace.passphrase, passphrase_length);

  // Metadata lives inside AEAD-protected inner payload
  inner_header.compression = final_compression_mode;
  inner_header.name = (const uint8_t *)g_cli_workspace.payload_name;
  inner_header.name_len = (uint16_t)payload_name_length;
  inner_header.mime = mime_octet_stream;
  inner_header.mime_len = (uint16_t)sizeof(mime_octet_stream);

  // Inner container is encrypted as one authenticated blob
  result_code = plainsight_container_pack_inner(&inner_header, g_cli_workspace.inner,
                                                sizeof(g_cli_workspace.inner), &inner_length);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  kdf_params.opslimit = (uint64_t)crypto_pwhash_OPSLIMIT_INTERACTIVE;
  kdf_params.memlimit = (uint64_t)crypto_pwhash_MEMLIMIT_INTERACTIVE;
  kdf_params.alg = (uint16_t)crypto_pwhash_ALG_ARGON2ID13;

  outer_header.version = PLAINSIGHT_CONTAINER_VERSION;
  outer_header.kdf_alg = kdf_params.alg;
  outer_header.kdf_opslimit = kdf_params.opslimit;
  outer_header.kdf_memlimit = kdf_params.memlimit;

  // Outer header fields needed for KDF and decrypt are public by design
  result_code = plainsight_crypto_fill_random(outer_header.salt, sizeof(outer_header.salt));
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  result_code = plainsight_crypto_fill_random(outer_header.nonce, sizeof(outer_header.nonce));
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  result_code = plainsight_crypto_derive_key(g_cli_workspace.passphrase, passphrase_length, outer_header.salt,
                                             &kdf_params, encryption_key);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  result_code = plainsight_crypto_encrypt(encryption_key, outer_header.nonce, g_cli_workspace.inner,
                                          inner_length, g_cli_workspace.ciphertext,
                                          sizeof(g_cli_workspace.ciphertext), &ciphertext_length);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  outer_header.ciphertext_len = ciphertext_length;

  result_code = plainsight_container_pack_outer(&outer_header, g_cli_workspace.ciphertext, ciphertext_length,
                                                g_cli_workspace.container, sizeof(g_cli_workspace.container),
                                                &container_length);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Seed includes LSB-masked cover bytes so mapping is image-specific
  result_code = plainsight_crypto_seed_from_passphrase_and_cover(
      g_cli_workspace.passphrase, passphrase_length, g_cli_workspace.image.pixels,
      g_cli_workspace.image.data_len, embed_seed);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Embedding happens only after encryption output is finalized
  result_code = plainsight_embed_payload(options->method, g_cli_workspace.image.pixels,
                                         g_cli_workspace.image.data_len, g_cli_workspace.image.channels,
                                         g_cli_workspace.container, container_length, embed_seed);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Store stego output atomically so failures do not leave partial images
  result_code = plainsight_cli_store_image_atomic(options->output_path, &g_cli_workspace.image);
  if (result_code != PLAINSIGHT_OK) {
    (void)fputs("hide output write failed: target path: ", stderr);
    (void)fputs(options->output_path, stderr);
    (void)fputc('\n', stderr);
    (void)fputs(
        "  hint: output must be a writable, non-existing lossless image path (.png, .jxl, .bmp, .ppm)\n",
        stderr);
  }

cleanup:
  // Always wipe transient secrets regardless of success or failure
  plainsight_secure_zero(embed_seed, sizeof(embed_seed));
  plainsight_secure_zero(encryption_key, sizeof(encryption_key));
  plainsight_secure_zero(g_cli_workspace.payload, sizeof(g_cli_workspace.payload));
  plainsight_secure_zero(g_cli_workspace.payload_name, sizeof(g_cli_workspace.payload_name));
  plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
  plainsight_secure_zero(g_cli_workspace.ciphertext, sizeof(g_cli_workspace.ciphertext));

  if (passphrase_lock_result == PLAINSIGHT_OK) {
    (void)plainsight_secure_unlock(g_cli_workspace.passphrase, passphrase_length);
  } else {
    plainsight_secure_zero(g_cli_workspace.passphrase, passphrase_length);
  }

  return result_code;

cleanup_without_secret:
  plainsight_secure_zero(g_cli_workspace.payload, sizeof(g_cli_workspace.payload));
  plainsight_secure_zero(g_cli_workspace.payload_name, sizeof(g_cli_workspace.payload_name));
  plainsight_secure_zero(g_cli_workspace.ciphertext, sizeof(g_cli_workspace.ciphertext));
  return result_code;
}
