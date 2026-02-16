#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/container.h"

// This file tests container packing and parsing
// It verifies that pack -> parse preserves bytes and lengths exactly

static uint8_t g_inner_buf[PLAINSIGHT_MAX_INNER_BYTES];
static uint8_t g_outer_buf[PLAINSIGHT_MAX_CONTAINER_BYTES];

static int check_true(int condition, const char *message) {
    // Keep failure reporting minimal and deterministic
    if (!condition) {
        (void)fputs(message, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // Fixed buffers are used so the test does not depend on heap allocation behavior
    uint8_t payload[32];
    uint8_t salt[16];
    uint8_t nonce[24];
    const uint8_t *ciphertext_view = NULL;
    size_t inner_len = 0u;
    size_t outer_len = 0u;
    size_t ciphertext_len = 0u;
    plainsight_inner_header inner;
    plainsight_inner_header inner_view;
    plainsight_outer_header outer;
    plainsight_outer_header outer_view;
    size_t i = 0u;

    // Fill test data with a predictable pattern
    // Patterns make it easy to spot truncation, reordering, or zeroing bugs
    for (i = 0u; i < sizeof(payload); i++) {
        payload[i] = (uint8_t)(i + 1u);
    }
    for (i = 0u; i < sizeof(salt); i++) {
        salt[i] = (uint8_t)(0xA0u + i);
    }
    for (i = 0u; i < sizeof(nonce); i++) {
        nonce[i] = (uint8_t)(0xB0u + i);
    }

    // Inner header describes the payload and is serialized into bytes
    inner.compression = 0u;
    inner.name = (const uint8_t *)"payload.bin";
    inner.name_len = 11u;
    inner.mime = (const uint8_t *)"application/octet-stream";
    inner.mime_len = 24u;
    inner.payload = payload;
    inner.payload_len = sizeof(payload);

    // Pack inner structure into bytes
    if (!check_true(plainsight_container_pack_inner(&inner, g_inner_buf, sizeof(g_inner_buf), &inner_len) == PLAINSIGHT_OK,
                    "inner pack failed")) {
        return 1;
    }

    // Parse bytes back into a view and confirm lengths and pointers make sense
    if (!check_true(plainsight_container_parse_inner(g_inner_buf, inner_len, &inner_view) == PLAINSIGHT_OK,
                    "inner parse failed")) {
        return 1;
    }

    // Payload length is part of the inner header and must roundtrip exactly
    if (!check_true(inner_view.payload_len == inner.payload_len, "payload length mismatch")) {
        return 1;
    }

    // Payload bytes must match exactly
    if (!check_true(memcmp(inner_view.payload, payload, sizeof(payload)) == 0,
                    "payload bytes mismatch")) {
        return 1;
    }

    // Outer header holds KDF parameters and the AEAD nonce and wraps an encrypted inner blob
    // This test uses inner bytes as the outer "ciphertext" input to validate packing
    outer.version = PLAINSIGHT_CONTAINER_VERSION;
    outer.kdf_alg = 2u;
    outer.kdf_opslimit = 3u;
    outer.kdf_memlimit = 67108864u;
    outer.ciphertext_len = inner_len;
    for (i = 0u; i < sizeof(salt); i++) {
        outer.salt[i] = salt[i];
    }
    for (i = 0u; i < sizeof(nonce); i++) {
        outer.nonce[i] = nonce[i];
    }

    // Pack outer header plus ciphertext blob into one container
    if (!check_true(plainsight_container_pack_outer(&outer, g_inner_buf, inner_len, g_outer_buf, sizeof(g_outer_buf), &outer_len) == PLAINSIGHT_OK,
                    "outer pack failed")) {
        return 1;
    }

    // Parse outer header and recover the ciphertext view
    if (!check_true(plainsight_container_parse_outer(g_outer_buf, outer_len, &outer_view, &ciphertext_view, &ciphertext_len) == PLAINSIGHT_OK,
                    "outer parse failed")) {
        return 1;
    }

    // Ciphertext length is stored in the outer header and must match the blob size
    if (!check_true(ciphertext_len == inner_len, "ciphertext length mismatch")) {
        return 1;
    }

    // Ciphertext bytes must match the input bytes used during packing
    if (!check_true(memcmp(ciphertext_view, g_inner_buf, inner_len) == 0,
                    "ciphertext payload mismatch")) {
        return 1;
    }

    // Salt and nonce are not secret but must be preserved exactly
    if (!check_true(memcmp(outer_view.salt, salt, sizeof(salt)) == 0,
                    "salt mismatch")) {
        return 1;
    }

    if (!check_true(memcmp(outer_view.nonce, nonce, sizeof(nonce)) == 0,
                    "nonce mismatch")) {
        return 1;
    }

    return 0;
}
