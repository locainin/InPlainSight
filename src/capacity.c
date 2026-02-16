#include <stddef.h>
#include <stdint.h>

#include "../include/capacity.h"
#include "../include/container.h"

#define PLAINSIGHT_EMBED_LENGTH_PREFIX_BYTES 8u
#define PLAINSIGHT_INNER_FIXED_HEADER_BYTES 16u

plainsight_error plainsight_capacity_compute_lsb(const plainsight_capacity_input *input, plainsight_capacity_report *report) {
    uint64_t scaled_cover_bytes = 0u;
    uint64_t metadata_bytes = 0u;
    uint64_t fixed_overhead_bytes = 0u;
    uint64_t total_overhead_bytes = 0u;
    uint64_t max_embedded_bytes = 0u;

    if (input == NULL || report == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (input->cover_data_bytes == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (input->lsb_bits == 0u) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (input->density_per_mille == 0u || input->density_per_mille > 1000u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Density decides how much of the cover is touched
    // Example: 500 means only half of the bytes are considered usable
    // Multiplication is checked before it happens to avoid wraparound
    if (input->cover_data_bytes > (UINT64_MAX / (uint64_t)input->density_per_mille)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    scaled_cover_bytes = input->cover_data_bytes * (uint64_t)input->density_per_mille;
    report->usable_cover_bytes = scaled_cover_bytes / 1000u;

    // Each usable byte can carry lsb_bits payload bits
    // With lsb_bits=1, one cover byte carries one payload bit
    // This math is conservative and is not meant to hide statistical detectability
    if (report->usable_cover_bytes > (UINT64_MAX / (uint64_t)input->lsb_bits)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    report->usable_carrier_bits = report->usable_cover_bytes * (uint64_t)input->lsb_bits;
    max_embedded_bytes = report->usable_carrier_bits / 8u;

    // payload_name_len and mime_len are included because they are stored inside the encrypted inner header
    // These bytes still consume cover capacity even though they are not part of the user payload
    if (input->payload_name_len > UINT64_MAX - (uint64_t)input->mime_len) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    metadata_bytes = (uint64_t)input->payload_name_len + (uint64_t)input->mime_len;

    // Overhead is data that must be embedded before real payload bytes
    // This includes the 64-bit embedded length prefix and container framing
    // The inner fixed header bytes account for encrypted metadata fields
    fixed_overhead_bytes = PLAINSIGHT_EMBED_LENGTH_PREFIX_BYTES + PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES +
                           PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES + PLAINSIGHT_INNER_FIXED_HEADER_BYTES;
    if (metadata_bytes > UINT64_MAX - fixed_overhead_bytes) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    total_overhead_bytes = fixed_overhead_bytes + metadata_bytes;
    report->overhead_bytes = total_overhead_bytes;

    // Payload capacity is whatever remains after overhead is reserved
    if (max_embedded_bytes <= total_overhead_bytes) {
        report->max_payload_by_cover_bytes = 0u;
        return PLAINSIGHT_OK;
    }

    // max_payload_by_cover_bytes is the cover-derived maximum and is further clamped by PLAINSIGHT_MAX_PAYLOAD_BYTES elsewhere
    report->max_payload_by_cover_bytes = max_embedded_bytes - total_overhead_bytes;
    return PLAINSIGHT_OK;
}
