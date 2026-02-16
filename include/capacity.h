#ifndef PLAINSIGHT_CAPACITY_H
#define PLAINSIGHT_CAPACITY_H

#include <stddef.h>
#include <stdint.h>

#include "error.h"

#ifdef __cplusplus
extern "C" {
#endif

// Input values used by the capacity estimator for pixel-domain LSB embedding
typedef struct plainsight_capacity_input {
    // Decoded cover byte length available to embedding
    uint64_t cover_data_bytes;
    // Number of LSBs used per cover byte
    uint8_t lsb_bits;
    // Density as per-mille to avoid floating math in core calculation
    uint16_t density_per_mille;
    // Inner metadata lengths that consume capacity
    size_t payload_name_len;
    size_t mime_len;
} plainsight_capacity_input;

// Computed capacity report shared by CLI hide and info commands
typedef struct plainsight_capacity_report {
    // Cover bytes actually eligible for embedding after density filtering
    uint64_t usable_cover_bytes;
    // Total carrier bits from usable bytes and selected lsb_bits
    uint64_t usable_carrier_bits;
    // Non-payload byte overhead for prefix and container framing
    uint64_t overhead_bytes;
    // Max payload bytes that fit this cover before project cap clamping
    uint64_t max_payload_by_cover_bytes;
} plainsight_capacity_report;

// Computes cover-dependent LSB capacity with overflow-safe arithmetic
plainsight_error plainsight_capacity_compute_lsb(const plainsight_capacity_input *input, plainsight_capacity_report *report);

#ifdef __cplusplus
}
#endif

#endif
