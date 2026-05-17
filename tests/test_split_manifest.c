// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/split/manifest.h"

// This file tests split manifest packing and parsing
// The manifest is plaintext inside the encrypted shard 0 data and must be strictly validated

static uint8_t g_manifest_bytes[PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES];

static int check_true(int condition, const char *message_text) {
    // Print a single reason to stderr and stop
    if (!condition) {
        (void)fputs(message_text, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // set_id ties a group of shards together and helps avoid mixing unrelated shard sets
    uint8_t set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
    uint32_t per_shard_plain_len[3] = {100u, 120u, 80u};
    uint64_t per_shard_cipher_len[3] = {116u, 136u, 96u};
    plainsight_split_manifest_view parsed_manifest;
    size_t packed_len = 0u;
    size_t index = 0u;
    uint8_t tampered_manifest[PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES];

    // Deterministic set id bytes for stable assertions
    for (index = 0u; index < PLAINSIGHT_SPLIT_SET_ID_BYTES; index++) {
        set_id[index] = (uint8_t)(0x20u + index);
    }

    // Pack a manifest that includes per-shard ciphertext lengths
    // This lets extraction cross-check outer headers before attempting decryption
    if (!check_true(plainsight_split_manifest_pack(PLAINSIGHT_SPLIT_MANIFEST_VERSION,
                                           PLAINSIGHT_SPLIT_MANIFEST_FLAG_HAS_CIPHER_LEN,
                                           set_id,
                                           300u,
                                           0u,
                                           120u,
                                           3u,
                                           per_shard_plain_len,
                                           per_shard_cipher_len,
                                           g_manifest_bytes,
                                           sizeof(g_manifest_bytes),
                                           &packed_len) == PLAINSIGHT_OK,
                    "manifest pack failed")) {
        return 1;
    }

    // Parse back and confirm critical fields
    if (!check_true(plainsight_split_manifest_parse(g_manifest_bytes, packed_len, &parsed_manifest) == PLAINSIGHT_OK,
                    "manifest parse failed")) {
        return 1;
    }

    if (!check_true(parsed_manifest.shard_count == 3u, "manifest shard count mismatch")) {
        return 1;
    }
    if (!check_true(parsed_manifest.total_plaintext_len == 300u, "manifest total len mismatch")) {
        return 1;
    }

    // Unknown version must fail closed
    // Forward compatibility is handled by bumping version and teaching the parser explicitly
    for (index = 0u; index < packed_len; index++) {
        tampered_manifest[index] = g_manifest_bytes[index];
    }
    tampered_manifest[4] = 99u;

    if (!check_true(plainsight_split_manifest_parse(tampered_manifest,
                                            packed_len,
                                            &parsed_manifest) == PLAINSIGHT_ERR_BAD_FORMAT,
                    "unknown manifest version should fail")) {
        return 1;
    }

    return 0;
}
