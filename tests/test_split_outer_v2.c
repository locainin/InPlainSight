// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/split/outer_v2.h"

// This file tests the split outer header v2 packing and parsing helpers
// The outer header is not secret, but it must be strict and fail closed on unknown flags

static uint8_t g_buffer[PLAINSIGHT_SPLIT_OUTER_FIXED_BYTES + 64u];

static int check_true(int condition, const char *message_text) {
    // Keep output small and predictable for CI logs
    if (!condition) {
        (void)fputs(message_text, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // Build a header with a small ciphertext payload and validate pack -> parse
    plainsight_split_outer_v2 header;
    plainsight_split_outer_v2 parsed_header;
    const uint8_t *ciphertext_view = NULL;
    size_t packed_len = 0u;
    size_t ciphertext_len = 0u;
    size_t index = 0u;
    uint8_t ciphertext[64u];

    // Fixed header fields for a three-shard set
    header.version = PLAINSIGHT_SPLIT_OUTER_VERSION;
    header.flags = PLAINSIGHT_SPLIT_FLAG_HAS_MANIFEST;
    header.kdf_alg = 1u;
    header.kdf_opslimit = 2u;
    header.kdf_memlimit = 3u;
    header.shard_index = 0u;
    header.shard_count = 3u;
    header.ciphertext_len = 64u;

    // Fill salt, nonce, and set id with deterministic patterns
    for (index = 0u; index < 16u; index++) {
        header.salt[index] = (uint8_t)(0xA0u + index);
    }
    for (index = 0u; index < 24u; index++) {
        header.nonce[index] = (uint8_t)(0x40u + index);
    }
    for (index = 0u; index < PLAINSIGHT_SPLIT_SET_ID_BYTES; index++) {
        header.set_id[index] = (uint8_t)(0x10u + index);
    }
    for (index = 0u; index < sizeof(ciphertext); index++) {
        ciphertext[index] = (uint8_t)(index + 1u);
    }

    // Pack outer header and ciphertext into one byte buffer
    if (!check_true(plainsight_split_outer_v2_pack(&header,
                                           ciphertext,
                                           sizeof(ciphertext),
                                           g_buffer,
                                           sizeof(g_buffer),
                                           &packed_len) == PLAINSIGHT_OK,
                    "split v2 pack failed")) {
        return 1;
    }

    // Parse outer header back out and confirm ciphertext view and length
    if (!check_true(plainsight_split_outer_v2_parse(g_buffer,
                                            packed_len,
                                            &parsed_header,
                                            &ciphertext_view,
                                            &ciphertext_len) == PLAINSIGHT_OK,
                    "split v2 parse failed")) {
        return 1;
    }

    // Ciphertext view must reference the expected bytes
    if (!check_true(ciphertext_len == sizeof(ciphertext), "ciphertext length mismatch")) {
        return 1;
    }
    if (!check_true(memcmp(ciphertext_view, ciphertext, sizeof(ciphertext)) == 0,
                    "ciphertext bytes mismatch")) {
        return 1;
    }

    // Unknown flags must be rejected to keep parsing fail-closed
    // This blocks silent behavior changes when a future format adds new flags
    g_buffer[9] = (uint8_t)(PLAINSIGHT_SPLIT_FLAGS_KNOWN | 0x80u);
    if (!check_true(plainsight_split_outer_v2_parse(g_buffer,
                                            packed_len,
                                            &parsed_header,
                                            &ciphertext_view,
                                            &ciphertext_len) == PLAINSIGHT_ERR_BAD_FORMAT,
                    "unknown split flags should fail parse")) {
        return 1;
    }

    return 0;
}
