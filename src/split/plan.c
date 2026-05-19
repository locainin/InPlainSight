// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../../include/split/plan.h"
#include "../../include/split/manifest.h"

// Manifest layout size is deterministic
// Base bytes cover magic, version, reserved, set_id, totals, and fixed fields
#define PLAINSIGHT_SPLIT_MANIFEST_BASE_BYTES 44u

static plainsight_error plainsight_split_plan_manifest_len(uint32_t shard_count, size_t *manifest_len_out) {
    size_t manifest_len = 0u;

    if (manifest_len_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (shard_count == 0u || shard_count > PLAINSIGHT_MAX_SHARDS) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // This build always includes the per-shard ciphertext length table for strict cross-checking
    // manifest_len = base + (plain_len table) + (cipher_len table)
    // The table sizes are fixed so the manifest length can be planned without allocation
    manifest_len = (size_t)PLAINSIGHT_SPLIT_MANIFEST_BASE_BYTES + ((size_t)shard_count * 4u) +
                   ((size_t)shard_count * 8u);
    if (manifest_len > PLAINSIGHT_SPLIT_MANIFEST_MAX_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    *manifest_len_out = manifest_len;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_split_plan_compute(uint64_t payload_bytes,
                               uint64_t per_shard_plain_capacity,
                               plainsight_split_plan *plan_out) {
    uint32_t shard_count = 0u;
    size_t manifest_len = 0u;
    uint64_t shard0_data_capacity = 0u;
    uint64_t total_data_capacity;

    if (plan_out == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Split payload total is bounded so extraction cannot be forced to write unbounded output
    if (payload_bytes > (uint64_t)PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (per_shard_plain_capacity == 0u) {
        // No shard can store any payload bytes in this configuration
        return PLAINSIGHT_ERR_CAPACITY;
    }
    if (per_shard_plain_capacity > (uint64_t)PLAINSIGHT_MAX_SHARD_PLAINTEXT_BYTES) {
        // This caller contract keeps lengths compatible with fixed workspace buffers
        return PLAINSIGHT_ERR_ARGS;
    }

    // Start optimistic and grow until shard 0 can hold the manifest and the full payload fits
    // This loop is fast because the manifest grows linearly with shard_count
    // shard_count growth is bounded by PLAINSIGHT_MAX_SHARDS, so the loop cannot run forever
    shard_count = 1u;
    while (1) {
        plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

        result_code = plainsight_split_plan_manifest_len(shard_count, &manifest_len);
        if (result_code != PLAINSIGHT_OK) {
            return result_code;
        }

        // Shard 0 must reserve space for the manifest prefix inside plaintext
        if (per_shard_plain_capacity <= (uint64_t)manifest_len) {
            // Even an empty payload would not fit because the manifest cannot fit
            return PLAINSIGHT_ERR_CAPACITY;
        }
        shard0_data_capacity = per_shard_plain_capacity - (uint64_t)manifest_len;

        // total_data_capacity = shard0_data_capacity + (shard_count - 1) * per_shard_plain_capacity
        if ((uint64_t)(shard_count - 1u) > 0u) {
            uint64_t other_capacity = 0u;
            if ((uint64_t)(shard_count - 1u) > UINT64_MAX / per_shard_plain_capacity) {
                return PLAINSIGHT_ERR_TOO_LARGE;
            }
            other_capacity = (uint64_t)(shard_count - 1u) * per_shard_plain_capacity;
            if (other_capacity > UINT64_MAX - shard0_data_capacity) {
                return PLAINSIGHT_ERR_TOO_LARGE;
            }
            total_data_capacity = shard0_data_capacity + other_capacity;
        } else {
            total_data_capacity = shard0_data_capacity;
        }

        if (total_data_capacity >= payload_bytes) {
            // Enough room exists for all payload bytes across all shards
            break;
        }

        if (shard_count >= PLAINSIGHT_MAX_SHARDS) {
            return PLAINSIGHT_ERR_TOO_LARGE;
        }
        // Add one shard and try again
        // Growing the shard count increases both total capacity and manifest size
        shard_count++;
    }

    plan_out->shard_count = shard_count;
    plan_out->chunk_plain_len = (uint32_t)per_shard_plain_capacity;
    plan_out->shard0_max_plain_data_len = (uint32_t)shard0_data_capacity;
    plan_out->manifest_len = manifest_len;
    return PLAINSIGHT_OK;
}
