// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sodium.h>

#include "../include/crypto.h"

// This file tests AEAD additional authenticated data support
// AAD is not encrypted, but it must be bound to the ciphertext integrity check

static int check_true(int condition, const char *message_text) {
    // Fail fast with a clear one-line message
    if (!condition) {
        (void)fputs(message_text, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // These sizes match libsodium's AEAD API constants
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    uint8_t nonce[crypto_aead_xchacha20poly1305_ietf_NPUBBYTES];
    uint8_t plaintext[] = {(uint8_t)'h', (uint8_t)'e', (uint8_t)'l', (uint8_t)'l', (uint8_t)'o'};
    uint8_t aad[] = {(uint8_t)1u, (uint8_t)2u, (uint8_t)3u, (uint8_t)4u};
    uint8_t ciphertext[64];
    uint8_t decrypted[64];
    size_t ciphertext_len = 0u;
    size_t plaintext_len = 0u;

    // libsodium must be initialized before crypto helpers are used
    if (!check_true(plainsight_crypto_init() == PLAINSIGHT_OK, "crypto init failed")) {
        return 1;
    }

    // Use random key and nonce so this test does not rely on fixed values
    if (!check_true(plainsight_crypto_fill_random(key, sizeof(key)) == PLAINSIGHT_OK, "random key failed")) {
        return 1;
    }
    if (!check_true(plainsight_crypto_fill_random(nonce, sizeof(nonce)) == PLAINSIGHT_OK, "random nonce failed")) {
        return 1;
    }

    // Encrypt with a small AAD blob
    // The ciphertext output includes an authentication tag at the end
    if (!check_true(plainsight_crypto_encrypt_with_aad(key,
                                               nonce,
                                               plaintext,
                                               sizeof(plaintext),
                                               aad,
                                               sizeof(aad),
                                               ciphertext,
                                               sizeof(ciphertext),
                                               &ciphertext_len) == PLAINSIGHT_OK,
                    "encrypt with aad failed")) {
        return 1;
    }

    // Decrypt with the same AAD and confirm the plaintext bytes match
    if (!check_true(plainsight_crypto_decrypt_with_aad(key,
                                               nonce,
                                               ciphertext,
                                               ciphertext_len,
                                               aad,
                                               sizeof(aad),
                                               decrypted,
                                               sizeof(decrypted),
                                               &plaintext_len) == PLAINSIGHT_OK,
                    "decrypt with aad failed")) {
        return 1;
    }
    if (!check_true(plaintext_len == sizeof(plaintext), "aad decrypt length mismatch")) {
        return 1;
    }
    if (!check_true(memcmp(decrypted, plaintext, sizeof(plaintext)) == 0, "aad decrypt bytes mismatch")) {
        return 1;
    }

    // AAD tamper must force auth failure even when ciphertext bytes are unchanged
    // This confirms AAD is covered by the authentication tag
    aad[0] ^= 0x01u;
    if (!check_true(plainsight_crypto_decrypt_with_aad(key,
                                               nonce,
                                               ciphertext,
                                               ciphertext_len,
                                               aad,
                                               sizeof(aad),
                                               decrypted,
                                               sizeof(decrypted),
                                               &plaintext_len) == PLAINSIGHT_ERR_AUTH,
                    "aad tamper should fail authentication")) {
        return 1;
    }

    return 0;
}
