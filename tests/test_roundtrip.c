// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <sodium.h>

#include "../include/container.h"
#include "../include/crypto.h"
#include "../include/embed/embed.h"

// This file runs an end-to-end in-memory roundtrip
// It validates the core pipeline: pack -> encrypt -> embed -> extract -> decrypt -> parse

static uint8_t g_extracted_container[PLAINSIGHT_MAX_CONTAINER_BYTES];
static uint8_t g_decrypted[PLAINSIGHT_MAX_INNER_BYTES];
static uint8_t g_inner_buf[PLAINSIGHT_MAX_INNER_BYTES];
static uint8_t g_ciphertext[PLAINSIGHT_MAX_CIPHERTEXT_BYTES];
static uint8_t g_container[PLAINSIGHT_MAX_CONTAINER_BYTES];

static int check_true(int condition, const char *message) {
    // Keep failures easy to spot in test logs
    if (!condition) {
        (void)fputs(message, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // Use a synthetic cover buffer to avoid depending on any image codec in this test
    // The embed layer operates on raw bytes, so any buffer can act as a carrier in unit tests
    uint8_t cover[32768];
    uint8_t seed[32];
    uint8_t key[crypto_aead_xchacha20poly1305_ietf_KEYBYTES];
    plainsight_kdf_params params;
    plainsight_inner_header inner;
    plainsight_inner_header inner_view;
    plainsight_outer_header outer;
    const uint8_t *ciphertext_view = NULL;
    size_t inner_len = 0u;
    size_t ciphertext_len = 0u;
    size_t container_len = 0u;
    size_t extracted_len = 0u;
    size_t plain_len = 0u;
    size_t i = 0u;

    // Passphrase and payload are fixed so the test is deterministic
    static const uint8_t passphrase[] = {
        (uint8_t)'r', (uint8_t)'o', (uint8_t)'u', (uint8_t)'n', (uint8_t)'d',
        (uint8_t)'t', (uint8_t)'r', (uint8_t)'i', (uint8_t)'p'
    };

    static const uint8_t payload[] = {
        (uint8_t)'s', (uint8_t)'e', (uint8_t)'c', (uint8_t)'r', (uint8_t)'e', (uint8_t)'t',
        (uint8_t)'-', (uint8_t)'b', (uint8_t)'y', (uint8_t)'t', (uint8_t)'e', (uint8_t)'s'
    };

    static const uint8_t mime[] = {
        (uint8_t)'a', (uint8_t)'p', (uint8_t)'p', (uint8_t)'l', (uint8_t)'i',
        (uint8_t)'c', (uint8_t)'a', (uint8_t)'t', (uint8_t)'i', (uint8_t)'o',
        (uint8_t)'n', (uint8_t)'/', (uint8_t)'o', (uint8_t)'c', (uint8_t)'t',
        (uint8_t)'e', (uint8_t)'t', (uint8_t)'-', (uint8_t)'s', (uint8_t)'t',
        (uint8_t)'r', (uint8_t)'e', (uint8_t)'a', (uint8_t)'m'
    };

    // Initialize crypto backend first
    if (!check_true(plainsight_crypto_init() == PLAINSIGHT_OK, "crypto init failed")) {
        return 1;
    }

    // Fill the cover buffer with a repeatable pattern
    // This makes the derived stego seed stable across runs
    for (i = 0u; i < sizeof(cover); i++) {
        cover[i] = (uint8_t)(i * 13u + 7u);
    }

    // Build the inner header describing the payload
    inner.compression = 0u;
    inner.name = (const uint8_t *)"demo.bin";
    inner.name_len = 8u;
    inner.mime = mime;
    inner.mime_len = (uint16_t)sizeof(mime);
    inner.payload = payload;
    inner.payload_len = sizeof(payload);

    // Serialize inner header + payload into bytes
    if (!check_true(plainsight_container_pack_inner(&inner, g_inner_buf, sizeof(g_inner_buf), &inner_len) == PLAINSIGHT_OK,
                    "inner pack failed")) {
        return 1;
    }

    // KDF params are stored in the outer header so extraction can derive the same key
    params.opslimit = (uint64_t)crypto_pwhash_OPSLIMIT_INTERACTIVE;
    params.memlimit = (uint64_t)crypto_pwhash_MEMLIMIT_INTERACTIVE;
    params.alg = (uint16_t)crypto_pwhash_ALG_ARGON2ID13;

    // Outer header carries the salt and nonce used by AEAD
    outer.version = PLAINSIGHT_CONTAINER_VERSION;
    outer.kdf_alg = params.alg;
    outer.kdf_opslimit = params.opslimit;
    outer.kdf_memlimit = params.memlimit;

    // Deterministic salts and nonces are fine for a unit test
    // Real runs must use random values
    for (i = 0u; i < sizeof(outer.salt); i++) {
        outer.salt[i] = (uint8_t)(0x71u + i);
    }
    for (i = 0u; i < sizeof(outer.nonce); i++) {
        outer.nonce[i] = (uint8_t)(0x19u + i);
    }

    // Derive encryption key from passphrase and salt
    if (!check_true(plainsight_crypto_derive_key(passphrase, sizeof(passphrase), outer.salt, &params, key) == PLAINSIGHT_OK,
                    "derive key failed")) {
        return 1;
    }

    // Encrypt the inner bytes and record the ciphertext length
    if (!check_true(plainsight_crypto_encrypt(key,
                                      outer.nonce,
                                      g_inner_buf,
                                      inner_len,
                                      g_ciphertext,
                                      sizeof(g_ciphertext),
                                      &ciphertext_len) == PLAINSIGHT_OK,
                    "encrypt failed")) {
        return 1;
    }

    // The outer container stores the ciphertext length for parsing and bounds checking
    outer.ciphertext_len = ciphertext_len;

    // Pack outer header + ciphertext into one container blob
    if (!check_true(plainsight_container_pack_outer(&outer,
                                            g_ciphertext,
                                            ciphertext_len,
                                            g_container,
                                            sizeof(g_container),
                                            &container_len) == PLAINSIGHT_OK,
                    "outer pack failed")) {
        return 1;
    }

    // Derive a stego seed from passphrase and cover bytes
    // The same cover bytes are needed later for extraction
    if (!check_true(plainsight_crypto_seed_from_passphrase_and_cover(passphrase,
                                                             sizeof(passphrase),
                                                             cover,
                                                             sizeof(cover),
                                                             seed) == PLAINSIGHT_OK,
                    "seed failed")) {
        return 1;
    }

    // Embed container bytes into the carrier buffer
    // For LSB mode, the embedder modifies the least significant bits of selected carrier bytes
    if (!check_true(plainsight_embed_payload(PLAINSIGHT_EMBED_LSB,
                                     cover,
                                     sizeof(cover),
                                     3u,
                                     g_container,
                                     container_len,
                                     seed) == PLAINSIGHT_OK,
                    "embed failed")) {
        return 1;
    }

    // Extract the embedded container bytes back out of the carrier buffer
    if (!check_true(plainsight_extract_payload(PLAINSIGHT_EMBED_LSB,
                                       cover,
                                       sizeof(cover),
                                       3u,
                                       g_extracted_container,
                                       sizeof(g_extracted_container),
                                       &extracted_len,
                                       seed) == PLAINSIGHT_OK,
                    "extract failed")) {
        return 1;
    }

    // Extracted length should match the input container length for a successful roundtrip
    if (!check_true(extracted_len == container_len, "container length mismatch")) {
        return 1;
    }

    // Parse outer header and obtain the ciphertext view
    if (!check_true(plainsight_container_parse_outer(g_extracted_container,
                                             extracted_len,
                                             &outer,
                                             &ciphertext_view,
                                             &ciphertext_len) == PLAINSIGHT_OK,
                    "outer parse failed")) {
        return 1;
    }

    // Re-derive the same key on the extraction side
    if (!check_true(plainsight_crypto_derive_key(passphrase, sizeof(passphrase), outer.salt, &params, key) == PLAINSIGHT_OK,
                    "derive key second failed")) {
        return 1;
    }

    // Decrypt ciphertext back into the original inner bytes
    if (!check_true(plainsight_crypto_decrypt(key,
                                      outer.nonce,
                                      ciphertext_view,
                                      ciphertext_len,
                                      g_decrypted,
                                      sizeof(g_decrypted),
                                      &plain_len) == PLAINSIGHT_OK,
                    "decrypt failed")) {
        return 1;
    }

    // Parse inner bytes back into a view
    if (!check_true(plainsight_container_parse_inner(g_decrypted, plain_len, &inner_view) == PLAINSIGHT_OK,
                    "inner parse failed")) {
        return 1;
    }

    // Validate the payload content matches the original input bytes
    if (!check_true(inner_view.payload_len == sizeof(payload), "payload length mismatch")) {
        return 1;
    }

    if (!check_true(memcmp(inner_view.payload, payload, sizeof(payload)) == 0,
                    "payload mismatch")) {
        return 1;
    }

    return 0;
}
