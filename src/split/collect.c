// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../../include/split/collect.h"

plainsight_error plainsight_split_collect_validate_set(const plainsight_split_outer_v2 *headers,
                                       size_t header_count,
                                       const uint8_t expected_set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES],
                                       uint32_t *out_shard_count) {
    uint8_t seen_index[PLAINSIGHT_MAX_SHARDS];
    uint32_t shard_count = 0u;
    size_t header_index = 0u;
    size_t seen_total = 0u;
    int saw_shard_zero = 0;

    if (headers == NULL || expected_set_id == NULL || out_shard_count == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (header_count == 0u || header_count > PLAINSIGHT_MAX_SET_SCAN_IMAGES) {
        // Bounds prevent directory scans from consuming unbounded CPU
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    // seen_index is a small bitmap used to detect duplicates without heap allocations
    for (header_index = 0u; header_index < PLAINSIGHT_MAX_SHARDS; header_index++) {
        seen_index[header_index] = 0u;
    }

    // Validate each candidate shard before any decryption work
    // This keeps failures cheap and avoids triggering AEAD on obviously invalid sets
    for (header_index = 0u; header_index < header_count; header_index++) {
        const plainsight_split_outer_v2 *current_header = &headers[header_index];

        if (current_header->version != PLAINSIGHT_SPLIT_OUTER_VERSION) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
        if (current_header->shard_count == 0u || current_header->shard_count > PLAINSIGHT_MAX_SHARDS) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
        if (current_header->shard_index >= current_header->shard_count) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }

        // set_id mismatch means these shards come from different runs
        for (size_t set_id_index = 0u; set_id_index < PLAINSIGHT_SPLIT_SET_ID_BYTES; set_id_index++) {
            if (current_header->set_id[set_id_index] != expected_set_id[set_id_index]) {
                return PLAINSIGHT_ERR_BAD_FORMAT;
            }
        }

        // shard_count must be consistent across all shards in the set
        if (header_index == 0u) {
            shard_count = current_header->shard_count;
        } else if (current_header->shard_count != shard_count) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }

        // Duplicate indices indicate reused files or a tampered directory
        if (seen_index[current_header->shard_index] != 0u) {
            return PLAINSIGHT_ERR_BAD_FORMAT;
        }
        seen_index[current_header->shard_index] = 1u;
        seen_total++;

        if (current_header->shard_index == 0u) {
            saw_shard_zero = 1;
        }
    }

    // Shard zero is required because it carries the encrypted manifest
    if (saw_shard_zero == 0) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }
    // Complete set is required so extraction can assemble in strict index order
    if (seen_total != (size_t)shard_count) {
        return PLAINSIGHT_ERR_BAD_FORMAT;
    }

    *out_shard_count = shard_count;
    return PLAINSIGHT_OK;
}
