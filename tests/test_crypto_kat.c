#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sodium.h>

#include "../include/crypto.h"

// This file provides a small known-answer-style check for the crypto helpers
// The goal is to detect accidental nondeterminism in the wrapper logic when inputs are fixed

static int check_true(int condition, const char *message) {
    // Avoid any formatting logic so failures are easy to read in CI logs
    if (!condition) {
        (void)fputs(message, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // Use fixed values for salt, nonce, and plaintext so the output is reproducible
    uint8_t salt[crypto_pwhash_SALTBYTES];
    uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    uint8_t plaintext[128];
    uint8_t ciphertext_a[128 + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    uint8_t ciphertext_b[128 + crypto_aead_xchacha20poly1305_ietf_ABYTES];
    uint8_t decrypted[128];
    plainsight_kdf_params params;
    size_t ct_len_a = 0u;
    size_t ct_len_b = 0u;
    size_t pt_len = 0u;
    size_t i = 0u;
    static const uint8_t passphrase[] = {
        (uint8_t)'t', (uint8_t)'e', (uint8_t)'s', (uint8_t)'t',
        (uint8_t)'-', (uint8_t)'p', (uint8_t)'a', (uint8_t)'s',
        (uint8_t)'s'
    };

    // libsodium must be initialized before any crypto operations
    if (!check_true(plainsight_crypto_init() == PLAINSIGHT_OK, "crypto init failed")) {
        return 1;
    }

    // Fill buffers with deterministic patterns
    for (i = 0u; i < sizeof(salt); i++) {
        salt[i] = (uint8_t)(0x11u + i);
    }
    for (i = 0u; i < sizeof(nonce); i++) {
        nonce[i] = (uint8_t)(0x31u + i);
    }
    for (i = 0u; i < sizeof(plaintext); i++) {
        plaintext[i] = (uint8_t)(i ^ 0x5Au);
    }

    // Use interactive limits as a realistic default for tests
    params.opslimit = (uint64_t)crypto_pwhash_OPSLIMIT_INTERACTIVE;
    params.memlimit = (uint64_t)crypto_pwhash_MEMLIMIT_INTERACTIVE;
    params.alg = (uint16_t)crypto_pwhash_ALG_ARGON2ID13;

    // Derive a fixed-size AEAD key from passphrase and salt
    if (!check_true(plainsight_crypto_derive_key(passphrase,
                                         sizeof(passphrase),
                                         salt,
                                         &params,
                                         key) == PLAINSIGHT_OK,
                    "derive key failed")) {
        return 1;
    }

    // Encrypt twice with the same key, nonce, and plaintext
    // For a fixed nonce, AEAD output is deterministic, so both ciphertexts should match
    if (!check_true(plainsight_crypto_encrypt(key,
                                      nonce,
                                      plaintext,
                                      sizeof(plaintext),
                                      ciphertext_a,
                                      sizeof(ciphertext_a),
                                      &ct_len_a) == PLAINSIGHT_OK,
                    "encrypt a failed")) {
        return 1;
    }

    if (!check_true(plainsight_crypto_encrypt(key,
                                      nonce,
                                      plaintext,
                                      sizeof(plaintext),
                                      ciphertext_b,
                                      sizeof(ciphertext_b),
                                      &ct_len_b) == PLAINSIGHT_OK,
                    "encrypt b failed")) {
        return 1;
    }

    // Lengths must match when the inputs match
    if (!check_true(ct_len_a == ct_len_b, "cipher length mismatch")) {
        return 1;
    }

    // Bytes must match when the inputs match
    if (!check_true(memcmp(ciphertext_a, ciphertext_b, ct_len_a) == 0,
                    "cipher deterministic mismatch")) {
        return 1;
    }

    // Decrypt and confirm the plaintext is recovered exactly
    if (!check_true(plainsight_crypto_decrypt(key,
                                      nonce,
                                      ciphertext_a,
                                      ct_len_a,
                                      decrypted,
                                      sizeof(decrypted),
                                      &pt_len) == PLAINSIGHT_OK,
                    "decrypt failed")) {
        return 1;
    }

    // Decrypted length must match the original input length
    if (!check_true(pt_len == sizeof(plaintext), "plain length mismatch")) {
        return 1;
    }

    // Decrypted bytes must match the original input bytes
    if (!check_true(memcmp(decrypted, plaintext, sizeof(plaintext)) == 0,
                    "plain bytes mismatch")) {
        return 1;
    }

    return 0;
}
