// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

#include "cli_internal.h"

static int plainsight_cli_byte_is_safe_filename_char(uint8_t value) {
    if ((value >= (uint8_t)'a' && value <= (uint8_t)'z') ||
        (value >= (uint8_t)'A' && value <= (uint8_t)'Z') ||
        (value >= (uint8_t)'0' && value <= (uint8_t)'9')) {
        return 1;
    }

    return value == (uint8_t)'.' || value == (uint8_t)'_' || value == (uint8_t)'-' || value == (uint8_t)' ';
}

static plainsight_error plainsight_cli_sanitize_inner_name(const uint8_t *name_bytes,
                                           size_t name_len,
                                           char *out,
                                           size_t out_cap) {
    size_t index = 0u;
    size_t original_name_len = name_len;
    int has_non_dot_byte = 0;

    if (out == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (name_bytes == NULL || name_len == 0u) {
        return plainsight_io_copy_basename("payload.bin", out, out_cap, &index);
    }
    if (name_len >= out_cap) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    for (index = 0u; index < name_len; index++) {
        uint8_t byte_value = name_bytes[index];
        char output_char = '_';

        if (plainsight_cli_byte_is_safe_filename_char(byte_value) != 0 &&
            byte_value != (uint8_t)'/' &&
            byte_value != (uint8_t)'\\') {
            output_char = (char)byte_value;
        }

        if (index == 0u && output_char == '.') {
            output_char = '_';
        }

        if (output_char != '.') {
            has_non_dot_byte = 1;
        }
        out[index] = output_char;
    }

    while (original_name_len > 0u &&
           (out[original_name_len - 1u] == ' ' || out[original_name_len - 1u] == '.')) {
        out[original_name_len - 1u] = '_';
        has_non_dot_byte = 1;
        original_name_len--;
    }

    out[name_len] = '\0';
    if (has_non_dot_byte == 0) {
        return plainsight_io_copy_basename("payload.bin", out, out_cap, &index);
    }
    return PLAINSIGHT_OK;
}

static plainsight_error plainsight_cli_resolve_extract_output_path(const char *requested_output_path,
                                                   const char *payload_name,
                                                   char *out,
                                                   size_t out_cap) {
    struct stat output_metadata;
    size_t output_index = 0u;
    size_t dir_len = 0u;
    int has_directory = 0;
    char output_dir[1024];

    if (requested_output_path == NULL || payload_name == NULL || out == NULL || out_cap == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (stat(requested_output_path, &output_metadata) == 0 && S_ISDIR(output_metadata.st_mode)) {
        return plainsight_cli_join_dir_and_name(requested_output_path, payload_name, out, out_cap);
    }

    while (requested_output_path[output_index] != '\0') {
        if (requested_output_path[output_index] == '/') {
            dir_len = output_index;
            has_directory = 1;
        }
        output_index++;
    }

    if (output_index == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (has_directory == 0) {
        return plainsight_cli_join_dir_and_name(".", payload_name, out, out_cap);
    }

    if (dir_len == 0u) {
        return plainsight_cli_join_dir_and_name("/", payload_name, out, out_cap);
    }

    if (dir_len + 1u >= sizeof(output_dir)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    for (output_index = 0u; output_index < dir_len; output_index++) {
        output_dir[output_index] = requested_output_path[output_index];
    }
    output_dir[dir_len] = '\0';

    return plainsight_cli_join_dir_and_name(output_dir, payload_name, out, out_cap);
}

plainsight_error plainsight_cli_run_extract(const plainsight_extract_options *options) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    plainsight_error passphrase_lock_result = PLAINSIGHT_ERR_INTERNAL;
    plainsight_kdf_params kdf_params;
    plainsight_outer_header outer_header;
    plainsight_inner_header inner_view;
    const uint8_t *ciphertext_bytes = NULL;
    size_t ciphertext_length = 0u;
    uint8_t encryption_key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    uint8_t embed_seed[32];
    size_t passphrase_length = 0u;
    size_t container_length = 0u;
    size_t decrypted_length = 0u;
    size_t output_payload_length = 0u;
    const uint8_t *output_payload = NULL;
    int output_fd = -1;
    char temp_output_path[1024];
    char sanitized_payload_name[PLAINSIGHT_MAX_FILENAME_BYTES + 1u];
    char resolved_output_path[1024];

    if (options == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // temp_output_path is used during cleanup for best-effort unlink
    // Initializing it keeps the cleanup path side-effect free on early failures
    temp_output_path[0] = '\0';
    sanitized_payload_name[0] = '\0';
    resolved_output_path[0] = '\0';

    if (options->input_dir != NULL) {
        // Split extractor lives in a dedicated module to keep this file focused
        return plainsight_cli_run_extract_split(options);
    }

    result_code = plainsight_io_read_passphrase_file(options->passphrase_path,
                                             g_cli_workspace.passphrase,
                                             sizeof(g_cli_workspace.passphrase),
                                             &passphrase_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    passphrase_lock_result = plainsight_secure_lock(g_cli_workspace.passphrase, passphrase_length);

    // Load cover first because seed derivation depends on image bytes
    result_code = plainsight_cli_load_image(options->input_path, &g_cli_workspace.image);
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    result_code = plainsight_crypto_seed_from_passphrase_and_cover(g_cli_workspace.passphrase,
                                                            passphrase_length,
                                                            g_cli_workspace.image.pixels,
                                                            g_cli_workspace.image.data_len,
                                                            embed_seed);
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    // Any extraction parse/decrypt mismatch is normalized to auth failure
    result_code = plainsight_extract_payload(options->method,
                                     g_cli_workspace.image.pixels,
                                     g_cli_workspace.image.data_len,
                                     g_cli_workspace.image.channels,
                                     g_cli_workspace.container,
                                     sizeof(g_cli_workspace.container),
                                     &container_length,
                                     embed_seed);
    if (result_code != PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    // Parse outer bytes after extraction to recover KDF and nonce values
    result_code = plainsight_container_parse_outer(g_cli_workspace.container,
                                           container_length,
                                           &outer_header,
                                           &ciphertext_bytes,
                                           &ciphertext_length);
    if (result_code != PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    kdf_params.opslimit = outer_header.kdf_opslimit;
    kdf_params.memlimit = outer_header.kdf_memlimit;
    kdf_params.alg = outer_header.kdf_alg;

    result_code = plainsight_crypto_derive_key(
        g_cli_workspace.passphrase, passphrase_length, outer_header.salt, &kdf_params, encryption_key);
    if (result_code != PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    // Decrypt and authenticate in one step
    result_code = plainsight_crypto_decrypt(encryption_key,
                                    outer_header.nonce,
                                    ciphertext_bytes,
                                    ciphertext_length,
                                    g_cli_workspace.inner,
                                    sizeof(g_cli_workspace.inner),
                                    &decrypted_length);
    if (result_code != PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    result_code = plainsight_container_parse_inner(g_cli_workspace.inner, decrypted_length, &inner_view);
    if (result_code != PLAINSIGHT_OK) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }
    output_payload = inner_view.payload;
    output_payload_length = (size_t)inner_view.payload_len;

    if (inner_view.compression == PLAINSIGHT_COMPRESSION_ZSTD) {
        result_code = plainsight_decompress_zstd(inner_view.payload,
                                         (size_t)inner_view.payload_len,
                                         g_cli_workspace.payload,
                                         sizeof(g_cli_workspace.payload),
                                         &output_payload_length);
        if (result_code != PLAINSIGHT_OK) {
            result_code = PLAINSIGHT_ERR_AUTH;
            goto cleanup;
        }
        output_payload = g_cli_workspace.payload;
    } else if (inner_view.compression != PLAINSIGHT_COMPRESSION_NONE) {
        result_code = PLAINSIGHT_ERR_AUTH;
        goto cleanup;
    }

    result_code = plainsight_cli_sanitize_inner_name(inner_view.name,
                                             (size_t)inner_view.name_len,
                                             sanitized_payload_name,
                                             sizeof(sanitized_payload_name));
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    result_code = plainsight_cli_resolve_extract_output_path(options->output_path,
                                                     sanitized_payload_name,
                                                     resolved_output_path,
                                                     sizeof(resolved_output_path));
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    // Write only authenticated bytes and do it atomically
    // The temp file is exclusive so existing outputs are never overwritten
    result_code = plainsight_cli_open_temp_output_exclusive(resolved_output_path,
                                                    temp_output_path,
                                                    sizeof(temp_output_path),
                                                    &output_fd);
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    // Write all payload bytes before making the final output path visible
    result_code = plainsight_cli_write_all_fd(output_fd, output_payload, output_payload_length);
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

    // Close is checked so a short write or close error does not get masked
    if (close(output_fd) != 0) {
        output_fd = -1;
        result_code = PLAINSIGHT_ERR_IO;
        goto cleanup;
    }
    output_fd = -1;

    // Commit with link so an existing final path can never be overwritten
    result_code = plainsight_cli_commit_temp_output_exclusive(temp_output_path, resolved_output_path);
    if (result_code != PLAINSIGHT_OK) {
        goto cleanup;
    }

cleanup:
    // Cleanup mirrors hide cleanup so secret handling stays consistent
    plainsight_secure_zero(embed_seed, sizeof(embed_seed));
    plainsight_secure_zero(encryption_key, sizeof(encryption_key));
    plainsight_secure_zero(g_cli_workspace.payload, sizeof(g_cli_workspace.payload));
    plainsight_secure_zero(g_cli_workspace.payload_name, sizeof(g_cli_workspace.payload_name));
    plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
    plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));

    if (output_fd >= 0) {
        // Close best effort so cleanup does not leak descriptors
        (void)close(output_fd);
        output_fd = -1;
    }
    if (result_code != PLAINSIGHT_OK && temp_output_path[0] != '\0') {
        // Best effort cleanup so failures do not leave partial output files
        (void)unlink(temp_output_path);
    }

    if (passphrase_lock_result == PLAINSIGHT_OK) {
        (void)plainsight_secure_unlock(g_cli_workspace.passphrase, passphrase_length);
    } else {
        plainsight_secure_zero(g_cli_workspace.passphrase, passphrase_length);
    }

    return result_code;
}
