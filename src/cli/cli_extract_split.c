// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include <dirent.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cli_internal.h"

#include "../../include/split/collect.h"
#include "../../include/split/manifest.h"
#include "../../include/split/outer_v2.h"

// Split extract scans a directory for a complete shard set and assembles the payload
// Filenames are never trusted for ordering or shard identity

static int plainsight_cli_extension_is_supported_input(const char *name_text) {
    plainsight_image_format format = plainsight_image_detect_format_from_path(name_text);
    return format != PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;
}

static plainsight_error plainsight_cli_copy_name(const char *name_text, char *out, size_t out_cap) {
    size_t index = 0u;

    if (name_text == NULL || out == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Filename strings are only used for reopening the file later
    // The header fields drive ordering and validation
    while (name_text[index] != '\0') {
        if (index + 1u >= out_cap) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
        out[index] = name_text[index];
        index++;
    }
    out[index] = '\0';
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_cli_resolve_split_extract_output_path(const char *requested_output_path,
                                                                         const char *payload_file_name,
                                                                         char *out,
                                                                         size_t out_cap) {
    struct stat output_metadata;
    size_t path_len = 0u;

    if (requested_output_path == NULL || payload_file_name == NULL || out == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (stat(requested_output_path, &output_metadata) == 0 && S_ISDIR(output_metadata.st_mode)) {
        return plainsight_cli_join_dir_and_name(requested_output_path,
                                               payload_file_name,
                                               out,
                                               out_cap);
    }

    while (requested_output_path[path_len] != '\0') {
        if (path_len + 1u >= out_cap) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
        out[path_len] = requested_output_path[path_len];
        path_len++;
    }
    out[path_len] = '\0';
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_cli_guess_split_payload_name(const uint8_t *payload_bytes,
                                                                size_t payload_len,
                                                                char *out,
                                                                size_t out_cap) {
    const char *name_text = "recovered_payload.bin";
    size_t name_len = 0u;

    if (out == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (payload_bytes != NULL && payload_len >= 5u &&
        payload_bytes[0] == (uint8_t)'%' &&
        payload_bytes[1] == (uint8_t)'P' &&
        payload_bytes[2] == (uint8_t)'D' &&
        payload_bytes[3] == (uint8_t)'F' &&
        payload_bytes[4] == (uint8_t)'-') {
        name_text = "recovered_payload.pdf";
    } else if (payload_bytes != NULL && payload_len >= 8u &&
               payload_bytes[0] == 0x89u &&
               payload_bytes[1] == (uint8_t)'P' &&
               payload_bytes[2] == (uint8_t)'N' &&
               payload_bytes[3] == (uint8_t)'G' &&
               payload_bytes[4] == 0x0Du &&
               payload_bytes[5] == 0x0Au &&
               payload_bytes[6] == 0x1Au &&
               payload_bytes[7] == 0x0Au) {
        name_text = "recovered_payload.png";
    } else if (payload_bytes != NULL && payload_len >= 3u &&
               payload_bytes[0] == 0xFFu &&
               payload_bytes[1] == 0xD8u &&
               payload_bytes[2] == 0xFFu) {
        name_text = "recovered_payload.jpg";
    } else if (payload_bytes != NULL && payload_len >= 4u &&
               payload_bytes[0] == (uint8_t)'P' &&
               payload_bytes[1] == (uint8_t)'K' &&
               payload_bytes[2] == 0x03u &&
               payload_bytes[3] == 0x04u) {
        name_text = "recovered_payload.zip";
    }

    while (name_text[name_len] != '\0') {
        if (name_len + 1u >= out_cap) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
        out[name_len] = name_text[name_len];
        name_len++;
    }
    out[name_len] = '\0';
    return PLAINSIGHT_OK;
}

// Split extract depends on helper modules for:
// - file descriptor helpers used for atomic temp output
// - extracting one shard container from one stego image
// Keeping helpers outside this file keeps the directory scan and assembly logic readable

plainsight_error plainsight_cli_run_extract_split(const plainsight_extract_options *options) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    plainsight_error passphrase_lock_result = PLAINSIGHT_ERR_INTERNAL;
    int input_is_directory = 0;
    size_t passphrase_length = 0u;
    DIR *directory_handle = NULL;
    struct dirent *entry = NULL;
    uint8_t expected_set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
    uint32_t shard_count = 0u;
    int have_set = 0;
    int saw_other_set = 0;
    uint8_t shard_present[PLAINSIGHT_MAX_SHARDS];
    char shard_name_by_index[PLAINSIGHT_MAX_SHARDS][256];
    plainsight_split_outer_v2 shard_headers[PLAINSIGHT_MAX_SHARDS];
    uint32_t index = 0u;
    char path_buffer[1024];
    plainsight_split_manifest_view manifest_view;
    size_t manifest_len = 0u;
    uint32_t per_shard_plain_len[PLAINSIGHT_MAX_SHARDS];
    uint64_t per_shard_cipher_len[PLAINSIGHT_MAX_SHARDS];
    uint8_t aad_bytes[PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES];
    size_t aad_len = 0u;
    uint8_t stego_subkey[PLAINSIGHT_STEGO_SUBKEY_BYTES];
    uint8_t master_key[crypto_kdf_KEYBYTES];
    uint8_t shard_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    uint64_t written_total = 0u;
    int output_fd = -1;
    char temp_output_path[1024];
    char resolved_output_path[1024];
    char recovered_payload_name[64];
    size_t scanned_entries = 0u;

    if (options == NULL || options->input_dir == NULL || options->output_path == NULL ||
        options->passphrase_path == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // temp_output_path is used in cleanup as a guard for unlink
    // Initializing the first byte keeps cleanup side-effect free on early failures
    temp_output_path[0] = '\0';
    resolved_output_path[0] = '\0';
    recovered_payload_name[0] = '\0';

    result_code = plainsight_cli_path_is_directory(options->input_dir, &input_is_directory);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }
    if (input_is_directory == 0) {
        return PLAINSIGHT_ERR_ARGS;
    }

    result_code = plainsight_io_read_passphrase_file(options->passphrase_path,
                                             g_cli_workspace.passphrase,
                                             sizeof(g_cli_workspace.passphrase),
                                             &passphrase_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }
    passphrase_lock_result = plainsight_secure_lock(g_cli_workspace.passphrase, passphrase_length);

    // Split directories may contain many image candidates
    // Deriving this once avoids one Argon2id call per scanned file
    result_code = plainsight_crypto_derive_stego_subkey(g_cli_workspace.passphrase,
                                                passphrase_length,
                                                stego_subkey);
    if (result_code != PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    for (index = 0u; index < PLAINSIGHT_MAX_SHARDS; index++) {
        shard_present[index] = 0u;
        shard_name_by_index[index][0] = '\0';
        plainsight_secure_zero(&shard_headers[index], sizeof(shard_headers[index]));
        per_shard_plain_len[index] = 0u;
        per_shard_cipher_len[index] = 0u;
    }

    directory_handle = opendir(options->input_dir);
    if (directory_handle == NULL) {
        result_code = PLAINSIGHT_ERR_IO;
        goto cleanup;
    }

    // Pass 1: scan directory and collect one complete shard set by set_id
    while ((entry = readdir(directory_handle)) != NULL) {
        plainsight_split_outer_v2 outer;
        const uint8_t *ciphertext = NULL;
        size_t ciphertext_len = 0u;
        size_t container_len = 0u;
        uint32_t sid_index = 0u;

        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        if (!plainsight_cli_extension_is_supported_input(entry->d_name)) {
            continue;
        }

        // Cap scan work for untrusted directories
        // This prevents a directory with many files from consuming unbounded time
        scanned_entries++;
        if (scanned_entries > (size_t)PLAINSIGHT_MAX_SET_SCAN_IMAGES) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }

        if (plainsight_cli_join_dir_and_name(options->input_dir, entry->d_name, path_buffer, sizeof(path_buffer)) != PLAINSIGHT_OK) {
            continue;
        }

        result_code = plainsight_cli_split_extract_one_shard(path_buffer,
                                                     options->method,
                                                     stego_subkey,
                                                     g_cli_workspace.container,
                                                     sizeof(g_cli_workspace.container),
                                                     &container_len,
                                                     &outer,
                                                     &ciphertext,
                                                     &ciphertext_len);
        if (result_code != PLAINSIGHT_OK) {
            continue;
        }

        if (outer.shard_count == 0u || outer.shard_count > PLAINSIGHT_MAX_SHARDS) {
            continue;
        }
        if (outer.shard_index >= outer.shard_count) {
            continue;
        }
        // Shard 0 must carry a manifest prefix inside plaintext
        if (outer.shard_index == 0u && (outer.flags & PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST) == 0u) {
            continue;
        }

        if (have_set == 0) {
            shard_count = outer.shard_count;
            for (sid_index = 0u; sid_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; sid_index++) {
                expected_set_id[sid_index] = outer.set_id[sid_index];
            }
            have_set = 1;
        } else {
            if (outer.shard_count != shard_count) {
                continue;
            }
            for (sid_index = 0u; sid_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; sid_index++) {
                if (outer.set_id[sid_index] != expected_set_id[sid_index]) {
                    // A second distinct set_id means multiple payload sets exist in this directory
                    // Default behavior is fail closed to avoid accidental mixing
                    saw_other_set = 1;
                    goto next_entry;
                }
            }
        }

        if (shard_present[outer.shard_index] != 0u) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }

        if (plainsight_cli_copy_name(entry->d_name, shard_name_by_index[outer.shard_index], sizeof(shard_name_by_index[0])) !=
            PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_IO;
            goto cleanup;
        }

        shard_present[outer.shard_index] = 1u;
        shard_headers[outer.shard_index] = outer;

    next_entry:
        continue;
    }

    if (directory_handle != NULL) {
        (void)closedir(directory_handle);
        directory_handle = NULL;
    }

    if (have_set == 0) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    // Default behavior is fail closed when multiple distinct sets are detected
    // Verbose diagnostics can be added later, but the public error remains generic
    if (saw_other_set != 0) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    {
        uint32_t validated_shard_count = 0u;

        // Use the tested split collection validator as the source of set rules
        // The CLI still keeps names by index because files are reopened in pass 2
        result_code = plainsight_split_collect_validate_set(shard_headers,
                                                    (size_t)shard_count,
                                                    expected_set_id,
                                                    &validated_shard_count);
        if (result_code != PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }
        shard_count = validated_shard_count;
    }

    // Ensure KDF params and salt are consistent across shards before decrypt work
    // This prevents mixing shards from different runs that happen to share the same set_id
    for (index = 1u; index < shard_count; index++) {
        uint32_t salt_index = 0u;
        if (shard_headers[index].kdf_alg != shard_headers[0].kdf_alg ||
            shard_headers[index].kdf_opslimit != shard_headers[0].kdf_opslimit ||
            shard_headers[index].kdf_memlimit != shard_headers[0].kdf_memlimit) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }
        for (salt_index = 0u; salt_index < 16u; salt_index++) {
            if (shard_headers[index].salt[salt_index] != shard_headers[0].salt[salt_index]) {
                result_code = PLAINSIGHT_ERR_AUTH;
                goto cleanup;
            }
        }
    }

    // Derive the master key once from shard 0 KDF params and salt
    {
        plainsight_kdf_params kdf_params;
        kdf_params.opslimit = shard_headers[0].kdf_opslimit;
        kdf_params.memlimit = shard_headers[0].kdf_memlimit;
        kdf_params.alg = shard_headers[0].kdf_alg;
        result_code = plainsight_crypto_derive_master_key(g_cli_workspace.passphrase,
                                                  passphrase_length,
                                                  shard_headers[0].salt,
                                                  &kdf_params,
                                                  master_key);
        if (result_code != PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }
    }

    // Pass 2: decrypt shard 0, parse manifest, then decrypt remaining shards in strict index order
    for (index = 0u; index < shard_count; index++) {
        const uint8_t *ciphertext = NULL;
        size_t ciphertext_len = 0u;
        size_t container_len = 0u;
        plainsight_split_outer_v2 outer;
        size_t plaintext_len = 0u;
        uint32_t expected_plain_len = 0u;
        uint64_t expected_cipher_len = 0u;
        const uint8_t *plain_data = NULL;
        size_t plain_data_len = 0u;

        if (plainsight_cli_join_dir_and_name(options->input_dir,
                                     shard_name_by_index[index],
                                     path_buffer,
                                     sizeof(path_buffer)) != PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_IO;
            goto cleanup;
        }

        result_code = plainsight_cli_split_extract_one_shard(path_buffer,
                                                     options->method,
                                                     stego_subkey,
                                                     g_cli_workspace.container,
                                                     sizeof(g_cli_workspace.container),
                                                     &container_len,
                                                     &outer,
                                                     &ciphertext,
                                                     &ciphertext_len);
        if (result_code != PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }

        // Outer header must match the collected set id and expected indices
        // This prevents relying on filenames or directory iteration order
        if (outer.shard_index != index || outer.shard_count != shard_count) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }
        // Shard 0 must keep its manifest flag set
        // This defends against tampering between scan and decrypt passes
        if (index == 0u && (outer.flags & PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST) == 0u) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }
        {
            uint32_t sid_index = 0u;
            for (sid_index = 0u; sid_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; sid_index++) {
                if (outer.set_id[sid_index] != expected_set_id[sid_index]) {
                    result_code = PLAINSIGHT_ERR_AUTH;
                    goto cleanup;
                }
            }
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

        result_code = plainsight_crypto_decrypt_with_aad(shard_key,
                                                 outer.nonce,
                                                 ciphertext,
                                                 ciphertext_len,
                                                 aad_bytes,
                                                 aad_len,
                                                 g_cli_workspace.inner,
                                                 sizeof(g_cli_workspace.inner),
                                                 &plaintext_len);
        if (result_code != PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }

        if (index == 0u) {
            // Manifest is stored at the start of shard 0 plaintext
            // The manifest prefix tells how much data to read from each shard
            result_code = plainsight_split_manifest_parse_prefix(g_cli_workspace.inner,
                                                         plaintext_len,
                                                         &manifest_view,
                                                         &manifest_len);
            if (result_code != PLAINSIGHT_OK) {
                result_code = PLAINSIGHT_ERR_AUTH;
                goto cleanup;
            }
            if (manifest_view.shard_count != shard_count) {
                result_code = PLAINSIGHT_ERR_AUTH;
                goto cleanup;
            }
            {
                uint32_t sid_index = 0u;
                for (sid_index = 0u; sid_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; sid_index++) {
                    if (manifest_view.set_id[sid_index] != expected_set_id[sid_index]) {
                        result_code = PLAINSIGHT_ERR_AUTH;
                        goto cleanup;
                    }
                }
            }

            // Copy length tables out of shard 0 plaintext before it is wiped
            {
                uint32_t shard_table_index = 0u;
                for (shard_table_index = 0u; shard_table_index < shard_count; shard_table_index++) {
                    uint32_t plain_len_value = 0u;
                    uint64_t cipher_len_value = 0u;

                    // These helpers bounds check the index and validate manifest structure
                    if (plainsight_split_manifest_plain_len_at(&manifest_view, shard_table_index, &plain_len_value) != PLAINSIGHT_OK) {
                        result_code = PLAINSIGHT_ERR_AUTH;
                        goto cleanup;
                    }
                    if (plainsight_split_manifest_cipher_len_at(&manifest_view, shard_table_index, &cipher_len_value) != PLAINSIGHT_OK) {
                        result_code = PLAINSIGHT_ERR_AUTH;
                        goto cleanup;
                    }

                    per_shard_plain_len[shard_table_index] = plain_len_value;
                    per_shard_cipher_len[shard_table_index] = cipher_len_value;
                }
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
            // Shard 0 plaintext layout is: manifest prefix bytes then payload bytes
            if (plaintext_len != manifest_len + (size_t)expected_plain_len) {
                result_code = PLAINSIGHT_ERR_AUTH;
                goto cleanup;
            }
            plain_data = g_cli_workspace.inner + manifest_len;
            plain_data_len = (size_t)expected_plain_len;
        } else {
            // Non-zero shards carry only payload bytes
            if (plaintext_len != (size_t)expected_plain_len) {
                result_code = PLAINSIGHT_ERR_AUTH;
                goto cleanup;
            }
            plain_data = g_cli_workspace.inner;
            plain_data_len = (size_t)expected_plain_len;
        }

        if (output_fd < 0) {
            result_code = plainsight_cli_guess_split_payload_name(plain_data,
                                                          plain_data_len,
                                                          recovered_payload_name,
                                                          sizeof(recovered_payload_name));
            if (result_code != PLAINSIGHT_OK) {
                goto cleanup;
            }

            result_code = plainsight_cli_resolve_split_extract_output_path(options->output_path,
                                                                   recovered_payload_name,
                                                                   resolved_output_path,
                                                                   sizeof(resolved_output_path));
            if (result_code != PLAINSIGHT_OK) {
                goto cleanup;
            }

            // Open output only after authenticated shard 0 gives a useful fallback extension
            result_code = plainsight_cli_open_temp_output_exclusive(resolved_output_path,
                                                            temp_output_path,
                                                            sizeof(temp_output_path),
                                                            &output_fd);
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
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    result_code = PLAINSIGHT_OK;

cleanup:
    if (directory_handle != NULL) {
        (void)closedir(directory_handle);
    }
    if (output_fd >= 0) {
        (void)close(output_fd);
        output_fd = -1;
    }

    if (result_code != PLAINSIGHT_OK && temp_output_path[0] != '\0') {
        // Best effort cleanup so failures do not leave partial output files
        (void)unlink(temp_output_path);
    }

    plainsight_secure_zero(stego_subkey, sizeof(stego_subkey));
    plainsight_secure_zero(master_key, sizeof(master_key));
    plainsight_secure_zero(shard_key, sizeof(shard_key));
    plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
    plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));

    if (passphrase_lock_result == PLAINSIGHT_OK) {
        (void)plainsight_secure_unlock(g_cli_workspace.passphrase, passphrase_length);
    } else {
        plainsight_secure_zero(g_cli_workspace.passphrase, passphrase_length);
    }

    return result_code;
}
