#include <stddef.h>
#include <stdint.h>

#include "cli_internal.h"

plainsight_error plainsight_cli_run_extract(const plainsight_extract_options *options) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    plainsight_error passphrase_lock_result = PLAINSIGHT_OK;
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

    if (options == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
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

    // Only authenticated payload bytes are written to disk
    result_code = plainsight_io_write_file(
        options->output_path, inner_view.payload, (size_t)inner_view.payload_len);

cleanup:
    // Extraction cleanup mirrors hide cleanup for consistent secret handling
    plainsight_secure_zero(embed_seed, sizeof(embed_seed));
    plainsight_secure_zero(encryption_key, sizeof(encryption_key));
    plainsight_secure_zero(g_cli_workspace.payload, sizeof(g_cli_workspace.payload));
    plainsight_secure_zero(g_cli_workspace.payload_name, sizeof(g_cli_workspace.payload_name));
    plainsight_secure_zero(g_cli_workspace.inner, sizeof(g_cli_workspace.inner));
    plainsight_secure_zero(g_cli_workspace.container, sizeof(g_cli_workspace.container));

    if (passphrase_lock_result == PLAINSIGHT_OK) {
        (void)plainsight_secure_unlock(g_cli_workspace.passphrase, passphrase_length);
    } else {
        plainsight_secure_zero(g_cli_workspace.passphrase, passphrase_length);
    }

    return result_code;
}
