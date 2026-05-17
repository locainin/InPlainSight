// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/split/plan.h"

// This file tests split planning logic
// Planning decides how many shard images are required for a given payload length

static int check_true(int condition, const char *message_text) {
    // Use simple stderr output so failures show up clearly under make test
    if (!condition) {
        (void)fputs(message_text, stderr);
        (void)fputs("\n", stderr);
        return 0;
    }
    return 1;
}

int main(void) {
    // Plan output is a small struct that describes shard count and shard 0 layout details
    plainsight_split_plan plan;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    // 1000 bytes per shard can hold a 56-byte manifest and up to 944 bytes of data in shard 0
    // This test ensures the manifest overhead is modeled exactly
    result_code = plainsight_split_plan_compute(1u, 1000u, &plan);
    if (!check_true(result_code == PLAINSIGHT_OK, "plan compute failed for tiny payload")) {
        return 1;
    }
    if (!check_true(plan.shard_count == 1u, "tiny payload should require one shard")) {
        return 1;
    }
    if (!check_true(plan.manifest_len == 56u, "manifest length mismatch for shard_count=1")) {
        return 1;
    }
    if (!check_true(plan.shard0_max_plain_data_len == 944u, "shard0 capacity mismatch for shard_count=1")) {
        return 1;
    }

    // Payload length above 1944 requires three shards because manifest size grows with shard_count
    // This checks that the iterative shard_count calculation converges correctly
    result_code = plainsight_split_plan_compute(2000u, 1000u, &plan);
    if (!check_true(result_code == PLAINSIGHT_OK, "plan compute failed for multi-shard payload")) {
        return 1;
    }
    if (!check_true(plan.shard_count == 3u, "payload=2000 should require three shards")) {
        return 1;
    }
    if (!check_true(plan.manifest_len == 80u, "manifest length mismatch for shard_count=3")) {
        return 1;
    }

    // Per-shard capacity smaller than manifest should fail with capacity error
    // This prevents the planner from returning a plan that cannot be embedded
    result_code = plainsight_split_plan_compute(1u, 32u, &plan);
    if (!check_true(result_code == PLAINSIGHT_ERR_CAPACITY, "expected capacity failure when shard too small for manifest")) {
        return 1;
    }

    return 0;
}
