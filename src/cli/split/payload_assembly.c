// InPlainSight C module
// Authenticated split shard assembly lives here so directory scanning stays separate

#include <stddef.h>
#include <stdint.h>

#include <unistd.h>

#include "headers/shard_set.h"

#include "../../../include/split/manifest.h"

static int plainsight_cli_same_set_id(const uint8_t left[PLAINSIGHT_SPLIT_SET_ID_BYTES],
                                      const uint8_t right[PLAINSIGHT_SPLIT_SET_ID_BYTES]) {
  for (uint32_t index = 0u; index < PLAINSIGHT_SPLIT_SET_ID_BYTES; index++) {
    if (left[index] != right[index]) {
      return 0;
    }
  }
  return 1;
}

static int plainsight_cli_same_salt(const uint8_t left[16], const uint8_t right[16]) {
  for (uint32_t index = 0u; index < 16u; index++) {
    if (left[index] != right[index]) {
      return 0;
    }
  }
  return 1;
}

static plainsight_error
plainsight_cli_split_validate_kdf_group(const plainsight_cli_split_shard_set *split_set) {
  for (uint32_t index = 1u; index < split_set->shard_count; index++) {
    const plainsight_split_outer_v2 *current = &split_set->shard_headers[index];
    const plainsight_split_outer_v2 *first = &split_set->shard_headers[0];

    if (current->kdf_alg != first->kdf_alg || current->kdf_opslimit != first->kdf_opslimit ||
        current->kdf_memlimit != first->kdf_memlimit ||
        plainsight_cli_same_salt(current->salt, first->salt) == 0) {
      return PLAINSIGHT_ERR_AUTH;
    }
  }
  return PLAINSIGHT_OK;
}

static plainsight_error
plainsight_cli_split_derive_master_key(const plainsight_cli_split_shard_set *split_set,
                                       size_t passphrase_length, uint8_t master_key[crypto_kdf_KEYBYTES]) {
  plainsight_kdf_params kdf_params;

  kdf_params.opslimit = split_set->shard_headers[0].kdf_opslimit;
  kdf_params.memlimit = split_set->shard_headers[0].kdf_memlimit;
  kdf_params.alg = split_set->shard_headers[0].kdf_alg;

  return plainsight_crypto_derive_master_key(g_cli_workspace.passphrase, passphrase_length,
                                             split_set->shard_headers[0].salt, &kdf_params, master_key);
}

static plainsight_error
plainsight_cli_split_copy_manifest_lengths(const plainsight_split_manifest_view *manifest_view,
                                           uint32_t shard_count,
                                           uint32_t per_shard_plain_len[PLAINSIGHT_MAX_SHARDS],
                                           uint64_t per_shard_cipher_len[PLAINSIGHT_MAX_SHARDS]) {
  for (uint32_t index = 0u; index < shard_count; index++) {
    uint32_t plain_len_value = 0u;
    uint64_t cipher_len_value = 0u;

    // These helpers bounds check the index and validate manifest structure
    if (plainsight_split_manifest_plain_len_at(manifest_view, index, &plain_len_value) != PLAINSIGHT_OK) {
      return PLAINSIGHT_ERR_AUTH;
    }
    if (plainsight_split_manifest_cipher_len_at(manifest_view, index, &cipher_len_value) != PLAINSIGHT_OK) {
      return PLAINSIGHT_ERR_AUTH;
    }

    per_shard_plain_len[index] = plain_len_value;
    per_shard_cipher_len[index] = cipher_len_value;
  }
  return PLAINSIGHT_OK;
}

static plainsight_error
plainsight_cli_split_validate_outer_again(const plainsight_cli_split_shard_set *split_set,
                                          const plainsight_split_outer_v2 *outer, uint32_t expected_index) {
  if (outer->shard_index != expected_index || outer->shard_count != split_set->shard_count) {
    return PLAINSIGHT_ERR_AUTH;
  }
  // Shard 0 must keep its manifest flag set
  // This defends against tampering between scan and decrypt passes
  if (expected_index == 0u && (outer->flags & PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST) == 0u) {
    return PLAINSIGHT_ERR_AUTH;
  }
  if (plainsight_cli_same_set_id(outer->set_id, split_set->expected_set_id) == 0) {
    return PLAINSIGHT_ERR_AUTH;
  }
  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_split_assemble_payload(
    const plainsight_extract_options *options, const plainsight_cli_split_shard_set *split_set,
    size_t passphrase_length, const uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES]) {
  uint32_t per_shard_plain_len[PLAINSIGHT_MAX_SHARDS];
  uint64_t per_shard_cipher_len[PLAINSIGHT_MAX_SHARDS];
  uint8_t aad_bytes[PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES];
  uint8_t master_key[crypto_kdf_KEYBYTES];
  uint8_t shard_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
  char path_buffer[1024];
  char temp_output_path[1024];
  char resolved_output_path[1024];
  char recovered_payload_name[64];
  plainsight_split_manifest_view manifest_view;
  size_t manifest_len = 0u;
  size_t aad_len = 0u;
  uint64_t written_total = 0u;
  int output_fd = -1;
  plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

  if (options == NULL || split_set == NULL || stego_subkey == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  temp_output_path[0] = '\0';
  recovered_payload_name[0] = '\0';
  resolved_output_path[0] = '\0';
  plainsight_secure_zero(master_key, sizeof(master_key));
  plainsight_secure_zero(shard_key, sizeof(shard_key));

  for (uint32_t index = 0u; index < PLAINSIGHT_MAX_SHARDS; index++) {
    per_shard_plain_len[index] = 0u;
    per_shard_cipher_len[index] = 0u;
  }

  result_code = plainsight_cli_split_validate_kdf_group(split_set);
  if (result_code != PLAINSIGHT_OK) {
    goto cleanup;
  }

  // Derive the master key once from shard 0 KDF params and salt
  result_code = plainsight_cli_split_derive_master_key(split_set, passphrase_length, master_key);
  if (result_code != PLAINSIGHT_OK) {
    result_code = PLAINSIGHT_ERR_AUTH;
    goto cleanup;
  }

  // Decrypt shard 0 first so the manifest length table becomes available
  for (uint32_t index = 0u; index < split_set->shard_count; index++) {
    const uint8_t *ciphertext = NULL;
    const uint8_t *plain_data = NULL;
    size_t ciphertext_len = 0u;
    size_t container_len = 0u;
    size_t plaintext_len = 0u;
    size_t plain_data_len = 0u;
    uint32_t expected_plain_len = 0u;
    uint64_t expected_cipher_len = 0u;
    plainsight_split_outer_v2 outer;

    if (plainsight_cli_join_dir_and_name(options->input_dir, split_set->shard_name_by_index[index],
                                         path_buffer, sizeof(path_buffer)) != PLAINSIGHT_OK) {
      result_code = PLAINSIGHT_ERR_IO;
      goto cleanup;
    }

    result_code = plainsight_cli_split_read_shard(
        path_buffer, options->method, stego_subkey, g_cli_workspace.container,
        sizeof(g_cli_workspace.container), &container_len, &outer, &ciphertext, &ciphertext_len);
    if (result_code != PLAINSIGHT_OK) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }

    result_code = plainsight_cli_split_validate_outer_again(split_set, &outer, index);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }

    // AAD binding means tampering with outer header values causes AEAD failure
    result_code = plainsight_split_aad_serialize_outer_v2(&outer, aad_bytes, sizeof(aad_bytes), &aad_len);
    if (result_code != PLAINSIGHT_OK) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }

    result_code = plainsight_crypto_derive_shard_encryption_key(master_key, (uint64_t)index, shard_key);
    if (result_code != PLAINSIGHT_OK) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }

    result_code = plainsight_crypto_decrypt_with_aad(shard_key, outer.nonce, ciphertext, ciphertext_len,
                                                     aad_bytes, aad_len, g_cli_workspace.inner,
                                                     sizeof(g_cli_workspace.inner), &plaintext_len);
    if (result_code != PLAINSIGHT_OK) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }

    if (index == 0u) {
      // Manifest is stored at the start of shard 0 plaintext
      result_code = plainsight_split_manifest_parse_prefix(g_cli_workspace.inner, plaintext_len,
                                                           &manifest_view, &manifest_len);
      if (result_code != PLAINSIGHT_OK || manifest_view.shard_count != split_set->shard_count ||
          plainsight_cli_same_set_id(manifest_view.set_id, split_set->expected_set_id) == 0) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
      }

      result_code = plainsight_cli_split_copy_manifest_lengths(&manifest_view, split_set->shard_count,
                                                               per_shard_plain_len, per_shard_cipher_len);
      if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
      }
    }

    // Manifest tables are the source of truth for expected lengths
    expected_plain_len = per_shard_plain_len[index];
    expected_cipher_len = per_shard_cipher_len[index];
    if (outer.ciphertext_len != expected_cipher_len) {
      result_code = PLAINSIGHT_ERR_AUTH;
      goto cleanup;
    }

    if (index == 0u) {
      if (plaintext_len != manifest_len + (size_t)expected_plain_len) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
      }
      plain_data = g_cli_workspace.inner + manifest_len;
      plain_data_len = (size_t)expected_plain_len;
    } else {
      if (plaintext_len != (size_t)expected_plain_len) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
      }
      plain_data = g_cli_workspace.inner;
      plain_data_len = (size_t)expected_plain_len;
    }

    if (output_fd < 0) {
      result_code = plainsight_cli_split_guess_payload_name(
          plain_data, plain_data_len, recovered_payload_name, sizeof(recovered_payload_name));
      if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
      }

      result_code = plainsight_cli_split_resolve_output_path(
          options->output_path, recovered_payload_name, resolved_output_path, sizeof(resolved_output_path));
      if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
      }

      // Open output only after authenticated shard 0 gives a useful fallback extension
      result_code = plainsight_cli_open_temp_output_exclusive(resolved_output_path, temp_output_path,
                                                              sizeof(temp_output_path), &output_fd);
      if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
      }
    }

    result_code = plainsight_cli_write_all_fd(output_fd, plain_data, plain_data_len);
    if (result_code != PLAINSIGHT_OK) {
      goto cleanup;
    }
    if (written_total > UINT64_MAX - (uint64_t)plain_data_len) {
      result_code = PLAINSIGHT_ERR_INTERNAL;
      goto cleanup;
    }
    written_total += (uint64_t)plain_data_len;

    // Wipe shard key and decrypted plaintext before processing the next shard
    plainsight_secure_zero(shard_key, sizeof(shard_key));
    plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
    plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));
  }

  if (written_total != manifest_view.total_plaintext_len) {
    result_code = PLAINSIGHT_ERR_AUTH;
    goto cleanup;
  }
  if (close(output_fd) != 0) {
    output_fd = -1;
    result_code = PLAINSIGHT_ERR_IO;
    goto cleanup;
  }
  output_fd = -1;

  result_code = plainsight_cli_commit_temp_output_exclusive(temp_output_path, resolved_output_path);

cleanup:
  if (output_fd >= 0) {
    (void)close(output_fd);
  }
  if (result_code != PLAINSIGHT_OK && temp_output_path[0] != '\0') {
    // Best effort cleanup so failures do not leave partial output files
    (void)unlink(temp_output_path);
  }
  plainsight_secure_zero(master_key, sizeof(master_key));
  plainsight_secure_zero(shard_key, sizeof(shard_key));
  plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
  plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));
  return result_code;
}
