#ifndef PLAINSIGHT_SPLIT_PLAN_H
#define PLAINSIGHT_SPLIT_PLAN_H

#include <stddef.h>
#include <stdint.h>

#include "../error.h"
#include "outer_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Split planning output computed from cover capacity and payload length
typedef struct plainsight_split_plan {
  // Total number of shards required to store the payload
  uint32_t shard_count;
  // Nominal plaintext chunk size used by shards after shard 0
  uint32_t chunk_plain_len;
  // Maximum payload bytes that can be stored in shard 0 after reserving manifest space
  uint32_t shard0_max_plain_data_len;
  // Exact manifest prefix length in bytes for this shard_count and selected flags
  size_t manifest_len;
} plainsight_split_plan;

// Split planning input keeps similar-sized numeric values named at the call site
typedef struct plainsight_split_plan_input {
  // Original payload size before split wrapping
  uint64_t payload_bytes;
  // Payload bytes available in a normal non-zero shard
  uint64_t per_shard_plain_capacity;
} plainsight_split_plan_input;

// Computes shard_count and shard0 capacity for split mode
// per_shard_plain_capacity is the maximum payload bytes for non-zero shards
// The manifest is always stored inside shard 0 plaintext and consumes capacity there
plainsight_error plainsight_split_plan_compute(const plainsight_split_plan_input *input,
                                               plainsight_split_plan *plan_out);

#ifdef __cplusplus
}
#endif

#endif
