// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/split/collect.h"

// This file tests validation of a split shard set
// Validation should fail closed on duplicates, missing shards, or set id mismatches

static int check_true(int condition, const char *message_text) {
    // Minimal output keeps failures readable when run under sanitizers
    if (!condition) {
        (void)fputs(message_text, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // Use a small fixed header array to simulate a directory scan result
    plainsight_split_outer_v2 headers[3];
    uint8_t set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES];
    uint32_t shard_count = 0u;
    size_t index = 0u;

    // All shards in a set share the same set id
    for (index = 0u; index < PLAINSIGHT_SPLIT_SET_ID_BYTES; index++) {
        set_id[index] = (uint8_t)(0x90u + index);
    }

    // Create a valid set of three shards with shard indices 0, 1, 2
    for (index = 0u; index < 3u; index++) {
        headers[index].version = PLAINSIGHT_SPLIT_OUTER_VERSION;
        headers[index].flags = 0u;
        headers[index].shard_count = 3u;
        headers[index].shard_index = (uint32_t)index;
        headers[index].ciphertext_len = 64u;
        for (size_t sid = 0u; sid < PLAINSIGHT_SPLIT_SET_ID_BYTES; sid++) {
            headers[index].set_id[sid] = set_id[sid];
        }
    }

    // Validation should succeed for a complete set
    if (!check_true(plainsight_split_collect_validate_set(headers, 3u, set_id, &shard_count) == PLAINSIGHT_OK,
                    "split collect validation failed")) {
        return 1;
    }
    if (!check_true(shard_count == 3u, "split collect shard count mismatch")) {
        return 1;
    }

    // Duplicate index must fail validation
    // This prevents two files from claiming the same shard index
    headers[2].shard_index = 1u;
    if (!check_true(plainsight_split_collect_validate_set(headers, 3u, set_id, &shard_count) == PLAINSIGHT_ERR_BAD_FORMAT,
                    "duplicate shard index should fail")) {
        return 1;
    }

    return 0;
}
