// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include <stddef.h>
#include <stdint.h>

#include "../include/capacity.h"
#include "../include/container.h"

#define PLAINSIGHT_EMBED_LENGTH_PREFIX_BYTES 8u
#define PLAINSIGHT_MAX_LSB_BITS_PER_BYTE 8u

plainsight_error plainsight_capacity_compute_lsb(const plainsight_capacity_input *input,
                                                 plainsight_capacity_report *report) {
    plainsight_capacity_report local_report = {0};
    uint64_t scaled_cover_bytes = 0u;
    uint64_t metadata_bytes = 0u;
    uint64_t fixed_overhead_bytes = 0u;
    uint64_t total_overhead_bytes = 0u;
    uint64_t max_embedded_bytes = 0u;

    if (input == NULL || report == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (input->cover_data_bytes == 0u ||
        input->lsb_bits == 0u ||
        input->lsb_bits > PLAINSIGHT_MAX_LSB_BITS_PER_BYTE ||
        input->density_per_mille == 0u ||
        input->density_per_mille > 1000u) {
        return PLAINSIGHT_ERR_ARGS;
    }

    if (input->payload_name_len > PLAINSIGHT_MAX_FILENAME_BYTES ||
        input->mime_len > PLAINSIGHT_MAX_MIME_BYTES) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    if (input->cover_data_bytes > (UINT64_MAX / (uint64_t)input->density_per_mille)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    scaled_cover_bytes = input->cover_data_bytes * (uint64_t)input->density_per_mille;
    local_report.usable_cover_bytes = scaled_cover_bytes / 1000u;

    if (local_report.usable_cover_bytes > (UINT64_MAX / (uint64_t)input->lsb_bits)) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }
    local_report.usable_carrier_bits = local_report.usable_cover_bytes * (uint64_t)input->lsb_bits;
    max_embedded_bytes = local_report.usable_carrier_bits / 8u;

    metadata_bytes = (uint64_t)input->payload_name_len + (uint64_t)input->mime_len;

    fixed_overhead_bytes = PLAINSIGHT_EMBED_LENGTH_PREFIX_BYTES +
                           PLAINSIGHT_CONTAINER_OUTER_FIXED_BYTES +
                           PLAINSIGHT_CONTAINER_AEAD_TAG_BYTES +
                           PLAINSIGHT_CONTAINER_INNER_FIXED_BYTES;

    if (metadata_bytes > UINT64_MAX - fixed_overhead_bytes) {
        return PLAINSIGHT_ERR_TOO_LARGE;
    }

    total_overhead_bytes = fixed_overhead_bytes + metadata_bytes;
    local_report.overhead_bytes = total_overhead_bytes;

    if (max_embedded_bytes > total_overhead_bytes) {
        local_report.max_payload_by_cover_bytes = max_embedded_bytes - total_overhead_bytes;
    }

    *report = local_report;
    return PLAINSIGHT_OK;
}
