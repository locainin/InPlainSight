#ifndef PLAINSIGHT_SPLIT_COLLECT_H
#define PLAINSIGHT_SPLIT_COLLECT_H

#include <stddef.h>
#include <stdint.h>

#include "../error.h"
#include "outer_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Validates candidate shard headers after extraction and grouping by set_id
// This function is intentionally cheap and does not attempt decryption
plainsight_error plainsight_split_collect_validate_set(const plainsight_split_outer_v2 *headers,
                                       size_t header_count,
                                       const uint8_t expected_set_id[PLAINSIGHT_SPLIT_SET_ID_BYTES],
                                       uint32_t *out_shard_count);

#ifdef __cplusplus
}
#endif

#endif
