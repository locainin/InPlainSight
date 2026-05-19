// InPlainSight C module
// Split hide shard writer owns per-shard encryption, embedding, and output commits

#include <stddef.h>
#include <stdint.h>

#include "headers/shards.h"

plainsight_error
plainsight_cli_hide_shard_write_outputs(const plainsight_cli_hide_shard_write_context *context) {
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
  uint8_t shard_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
  uint8_t aad_bytes[PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES];
  char shard_name[256];
  char shard_path[1024];
  uint32_t shard_index = 0u;
  size_t aad_len = 0u;
  size_t plaintext_len = 0u;
  size_t ciphertext_len = 0u;
  size_t packed_len = 0u;
  size_t read_now = 0u;

  if (context == NULL || context->options == NULL || context->split_plan == NULL ||
      context->kdf_params == NULL || context->set_id == NULL || context->template_text == NULL ||
      context->shards_written == NULL || context->per_shard_plain_len == NULL ||
      context->per_shard_cipher_len == NULL || context->master_key == NULL || context->embed_seed == NULL ||
      context->outer_header == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (context->payload_file_descriptor < 0) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Shard loop embeds one encrypted container per output image
  for (shard_index = 0u; shard_index < context->split_plan->shard_count; shard_index++) {
    const uint32_t data_len = context->per_shard_plain_len[shard_index];
    const uint64_t expected_cipher_len = context->per_shard_cipher_len[shard_index];
    size_t set_id_index = 0u;

    // Reload cover for each shard so previous embeddings do not accumulate
    // This keeps image distortion per shard stable and predictable
    result_code = plainsight_cli_load_image(context->options->cover_path, &g_cli_workspace.image);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // Fill shard outer header fields
    context->outer_header->version = PLAINSIGHT_SPLIT_OUTER_VERSION;
    // Only shard 0 carries a manifest prefix inside its plaintext
    // Other shards are pure payload chunks
    context->outer_header->flags = (shard_index == 0u) ? PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST : 0u;
    context->outer_header->kdf_alg = context->kdf_params->alg;
    context->outer_header->kdf_opslimit = context->kdf_params->opslimit;
    context->outer_header->kdf_memlimit = context->kdf_params->memlimit;
    for (set_id_index = 0u; set_id_index < sizeof(context->outer_header->set_id); set_id_index++) {
      context->outer_header->set_id[set_id_index] = context->set_id[set_id_index];
    }
    context->outer_header->shard_index = shard_index;
    context->outer_header->shard_count = context->split_plan->shard_count;
    context->outer_header->ciphertext_len = expected_cipher_len;

    // Nonce is per-shard so ciphertexts remain independent
    result_code =
        plainsight_crypto_fill_random(context->outer_header->nonce, sizeof(context->outer_header->nonce));
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // Derive per-shard encryption key from the master key
    result_code =
        plainsight_crypto_derive_shard_encryption_key(context->master_key, (uint64_t)shard_index, shard_key);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // Build plaintext bytes for this shard
    if (shard_index == 0u) {
      // Shard 0 plaintext begins with the encrypted manifest, followed by payload bytes
      plaintext_len = context->manifest_len + (size_t)data_len;
      if (plaintext_len > sizeof(g_cli_workspace.inner)) {
        result_code = PLAINSIGHT_ERR_TOO_LARGE;
        goto cleanup;
      }
      result_code = plainsight_cli_read_exact_bytes(context->payload_file_descriptor,
                                                    g_cli_workspace.inner + context->manifest_len,
                                                    (size_t)data_len, &read_now);
    } else {
      // Non-zero shards are only raw payload chunk bytes
      plaintext_len = (size_t)data_len;
      if (plaintext_len > sizeof(g_cli_workspace.inner)) {
        result_code = PLAINSIGHT_ERR_TOO_LARGE;
        goto cleanup;
      }
      result_code = plainsight_cli_read_exact_bytes(context->payload_file_descriptor, g_cli_workspace.inner,
                                                    (size_t)data_len, &read_now);
    }
    if (result_code != PLAINSIGHT_OK || read_now != (size_t)data_len) {
      goto cleanup;
    }

    // Serialize canonical AAD bytes from the outer header
    // AAD binds shard index, counts, and KDF params to the ciphertext
    result_code = plainsight_split_aad_serialize_outer_v2(context->outer_header, aad_bytes, sizeof(aad_bytes),
                                                          &aad_len);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // Encrypt plaintext and bind outer header bytes as AAD
    result_code = plainsight_crypto_encrypt_with_aad(
        shard_key, context->outer_header->nonce, g_cli_workspace.inner, plaintext_len, aad_bytes, aad_len,
        g_cli_workspace.ciphertext, sizeof(g_cli_workspace.ciphertext), &ciphertext_len);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }
    if ((uint64_t)ciphertext_len != expected_cipher_len) {
      result_code = PLAINSIGHT_ERR_INTERNAL;
      goto cleanup;
    }

    // Pack outer header + ciphertext as the embedded payload bytes
    // Outer header is not secret, but it is authenticated via AEAD AAD
    result_code = plainsight_split_outer_v2_pack(context->outer_header, g_cli_workspace.ciphertext,
                                                 ciphertext_len, g_cli_workspace.container,
                                                 sizeof(g_cli_workspace.container), &packed_len);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // Embed into the LSB of the decoded pixels
    result_code = plainsight_embed_payload(context->options->method, g_cli_workspace.image.pixels,
                                           g_cli_workspace.image.data_len, g_cli_workspace.image.channels,
                                           g_cli_workspace.container, packed_len, context->embed_seed);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // Build output path and write image atomically
    // Atomic write is best effort and relies on preflight to prevent overwriting user files
    result_code = plainsight_cli_split_format_shard_filename(context->template_text, shard_index, shard_name,
                                                             sizeof(shard_name));
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }
    result_code = plainsight_cli_join_dir_and_name(context->options->output_dir, shard_name, shard_path,
                                                   sizeof(shard_path));
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    result_code = plainsight_cli_store_image_atomic(shard_path, &g_cli_workspace.image);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }
    (*context->shards_written)++;

    // Wipe per-shard plaintext and ciphertext bytes to reduce residual secret exposure
    // Wiping is done even on success so secrets do not linger between shard iterations
    plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
    plainsight_secure_zero(g_cli_workspace.ciphertext, sizeof(g_cli_workspace.ciphertext));
    plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));
    plainsight_secure_zero(shard_key, sizeof(shard_key));
  }

  result_code = PLAINSIGHT_OK;

cleanup:
  plainsight_secure_zero(shard_key, sizeof(shard_key));
  return result_code;
}
