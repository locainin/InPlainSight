#ifndef PLAINSIGHT_INFO_H
#define PLAINSIGHT_INFO_H

#include <stddef.h>
#include <stdint.h>

#include "capacity.h"
#include "error.h"
#include "image/image.h"
#include "io.h"
#include "split/outer_v2.h"

#ifdef __cplusplus
extern "C" {
#endif

// Aggregated planning report used by the info command and UI preflight flow
typedef struct plainsight_info_report {
    // JSON schema version for UI fail-closed parsing
    uint32_t plan_schema_version;
    // Cover geometry and decoded byte count
    plainsight_image_format format;
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    uint64_t decoded_bytes;
    // Method knobs included for forward-compatible planning output
    uint8_t lsb_bits;
    uint16_t density_per_mille;
    // Shared capacity report from method-specific math
    plainsight_capacity_report capacity;
    // Project safety caps from compile-time configuration
    uint64_t max_payload_per_shard_bytes;
    uint64_t max_total_payload_bytes;
    uint64_t max_image_bytes;
    // Optional payload-specific planning results
    int payload_provided;
    uint64_t payload_bytes;
    int fits_single;
    uint64_t required_shards;
    // Planner output used by UI for single vs split command selection
    int plan_mode_split;
    uint64_t per_shard_payload_estimate;
    int output_cap_risk;
} plainsight_info_report;

// Builds a full planning report for one cover and optional payload size
plainsight_error plainsight_info_build_report(const plainsight_image *image,
                              plainsight_image_format format,
                              uint8_t lsb_bits,
                              uint16_t density_per_mille,
                              size_t payload_name_len,
                              size_t mime_len,
                              int payload_provided,
                              uint64_t payload_bytes,
                              plainsight_info_report *report);

// Writes a stable JSON document for machine-readable preflight planning
plainsight_error plainsight_info_write_json_stdout(const plainsight_info_report *report, const char *limiting_factor_text);

#ifdef __cplusplus
}
#endif

#endif
