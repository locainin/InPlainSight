#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../include/info.h"
#include "../include/split/plan.h"

// info.c builds a machine-readable planning report for the UI and for CLI troubleshooting
// The goal is stable output and conservative math, not maximum compression or capacity tricks

static const char *plainsight_info_format_text(plainsight_image_format format) {
    // Short lowercase names keep JSON compact and stable for UI parsing
    if (format == PLAINSIGHT_IMAGE_FORMAT_PNG) {
        return "png";
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_JXL) {
        return "jxl";
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_BMP) {
        return "bmp";
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_PPM) {
        return "ppm";
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_JPEG) {
        return "jpeg";
    }
    if (format == PLAINSIGHT_IMAGE_FORMAT_WEBP) {
        return "webp";
    }
    return "unknown";
}

static int plainsight_info_format_has_output_cap_risk(plainsight_image_format format) {
    // PNG and JXL are compressed formats, encoded size is content-dependent
    if (format == PLAINSIGHT_IMAGE_FORMAT_PNG || format == PLAINSIGHT_IMAGE_FORMAT_JXL) {
        return 1;
    }
    // Lossy cover formats are decoded then typically written as PNG in current flow
    if (format == PLAINSIGHT_IMAGE_FORMAT_JPEG || format == PLAINSIGHT_IMAGE_FORMAT_WEBP) {
        return 1;
    }
    return 0;
}

static void plainsight_info_write_u64_stdout(uint64_t value) {
    uint8_t reversed_digits[32];
    size_t digit_count = 0u;
    size_t reverse_index = 0u;

    // Manual decimal writing avoids stdio formatting warnings and locale surprises
    // This keeps output predictable and avoids format-string style routines
    if (value == 0u) {
        (void)fputc('0', stdout);
        return;
    }

    while (value > 0u && digit_count < sizeof(reversed_digits)) {
        reversed_digits[digit_count] = (uint8_t)('0' + (uint8_t)(value % 10u));
        value /= 10u;
        digit_count++;
    }

    // Digits are written back in forward order
    while (reverse_index < digit_count) {
        size_t output_index = digit_count - 1u - reverse_index;
        (void)fputc((int)reversed_digits[output_index], stdout);
        reverse_index++;
    }
}

static void plainsight_info_write_u32_stdout(uint32_t value) {
    plainsight_info_write_u64_stdout((uint64_t)value);
}

static void plainsight_info_write_u16_stdout(uint16_t value) {
    plainsight_info_write_u64_stdout((uint64_t)value);
}

static void plainsight_info_write_u8_stdout(uint8_t value) {
    plainsight_info_write_u64_stdout((uint64_t)value);
}

static void plainsight_info_write_bool_stdout(int flag_value) {
    if (flag_value != 0) {
        (void)fputs("true", stdout);
        return;
    }
    (void)fputs("false", stdout);
}

static uint64_t plainsight_info_min_u64(uint64_t first_value, uint64_t second_value) {
    if (first_value < second_value) {
        return first_value;
    }
    return second_value;
}

plainsight_error plainsight_info_build_report(const plainsight_image *image,
                              plainsight_image_format format,
                              uint8_t lsb_bits,
                              uint16_t density_per_mille,
                              size_t payload_name_len,
                              size_t mime_len,
                              int payload_provided,
                              uint64_t payload_bytes,
                              plainsight_info_report *report) {
    plainsight_capacity_input capacity_input;
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;

    if (image == NULL || report == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // Copy method knobs into one input block for shared capacity math
    // This keeps info planning aligned with the hide preflight check
    capacity_input.cover_data_bytes = (uint64_t)image->data_len;
    capacity_input.lsb_bits = lsb_bits;
    capacity_input.density_per_mille = density_per_mille;
    capacity_input.payload_name_len = payload_name_len;
    capacity_input.mime_len = mime_len;

    result_code = plainsight_capacity_compute_lsb(&capacity_input, &report->capacity);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Cover geometry is included so the UI can display what was actually decoded
    report->format = format;
    report->plan_schema_version = 1u;
    report->width = image->width;
    report->height = image->height;
    report->channels = image->channels;
    report->decoded_bytes = (uint64_t)image->data_len;
    report->lsb_bits = lsb_bits;
    report->density_per_mille = density_per_mille;
    // Per-shard payload is capped by both cover math and hard project bounds
    report->max_payload_per_shard_bytes =
        plainsight_info_min_u64(report->capacity.max_payload_by_cover_bytes, (uint64_t)PLAINSIGHT_MAX_PAYLOAD_BYTES);
    report->max_total_payload_bytes = (uint64_t)PLAINSIGHT_MAX_TOTAL_PAYLOAD_BYTES;
    report->max_image_bytes = (uint64_t)PLAINSIGHT_MAX_IMAGE_BYTES;
    report->payload_provided = payload_provided;
    report->payload_bytes = payload_bytes;
    report->fits_single = 0;
    report->required_shards = 0u;
    report->plan_mode_split = 0;
    report->per_shard_payload_estimate = report->max_payload_per_shard_bytes;
    report->output_cap_risk = plainsight_info_format_has_output_cap_risk(format);

    if (payload_provided != 0) {
        // required_shards is computed via ceiling division
        // The split planner will later use the same underlying shard capacity value
        if (report->max_payload_per_shard_bytes > 0u) {
            uint64_t shard_capacity = report->max_payload_per_shard_bytes;
            plainsight_split_plan split_plan;
            uint64_t payload_rounded = 0u;

            // Ceiling division computes how many shards would be required
            // This is planning-only for now until split mode is implemented
            if (payload_bytes > UINT64_MAX - (shard_capacity - 1u)) {
                return PLAINSIGHT_ERR_TOO_LARGE;
            }
            payload_rounded = payload_bytes + shard_capacity - 1u;
            report->fits_single = payload_bytes <= shard_capacity ? 1 : 0;

            // When payload does not fit in one cover, split planning must account for shard-0 manifest space
            // This keeps required_shards aligned with the actual writer behavior
            if (report->fits_single == 0) {
                result_code = plainsight_split_plan_compute(payload_bytes, shard_capacity, &split_plan);
                if (result_code != PLAINSIGHT_OK) {
                    return result_code;
                }
                report->required_shards = (uint64_t)split_plan.shard_count;
                report->per_shard_payload_estimate = (uint64_t)split_plan.chunk_plain_len;
            } else {
                report->required_shards = payload_rounded / shard_capacity;
            }
        } else {
            report->required_shards = 0u;
            report->fits_single = 0;
        }

        // plan_mode_split indicates the CLI would be able to run split mode for this input
        // This field is used by the UI to select single vs split execution
        if (report->fits_single == 0 &&
            report->required_shards > 1u &&
            report->required_shards <= PLAINSIGHT_MAX_SHARDS &&
            payload_bytes <= report->max_total_payload_bytes) {
            report->plan_mode_split = 1;
        }
    }

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_info_write_json_stdout(const plainsight_info_report *report, const char *limiting_factor_text) {
    if (report == NULL || limiting_factor_text == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    // JSON is emitted in one stable schema so UI parsing remains predictable
    // Keys are intentionally ordered to keep logs and snapshots easy to compare
    // JSON output is compact by design so it stays readable in UI logs
    (void)fputs("{\"plan_schema_version\":", stdout);
    plainsight_info_write_u32_stdout(report->plan_schema_version);
    (void)fputs(",\"cover\":{", stdout);
    (void)fputs("\"format\":\"", stdout);
    (void)fputs(plainsight_info_format_text(report->format), stdout);
    (void)fputs("\",\"width\":", stdout);
    plainsight_info_write_u32_stdout(report->width);
    (void)fputs(",\"height\":", stdout);
    plainsight_info_write_u32_stdout(report->height);
    (void)fputs(",\"channels\":", stdout);
    plainsight_info_write_u8_stdout(report->channels);
    (void)fputs(",\"decoded_bytes\":", stdout);
    plainsight_info_write_u64_stdout(report->decoded_bytes);
    (void)fputs("},\"method\":{", stdout);
    (void)fputs("\"name\":\"lsb\",\"lsb_bits\":", stdout);
    plainsight_info_write_u8_stdout(report->lsb_bits);
    (void)fputs(",\"density_per_mille\":", stdout);
    plainsight_info_write_u16_stdout(report->density_per_mille);
    (void)fputs("},\"caps\":{", stdout);
    (void)fputs("\"max_payload_per_shard\":", stdout);
    plainsight_info_write_u64_stdout(report->max_payload_per_shard_bytes);
    (void)fputs(",\"max_total_payload\":", stdout);
    plainsight_info_write_u64_stdout(report->max_total_payload_bytes);
    (void)fputs(",\"max_image_bytes\":", stdout);
    plainsight_info_write_u64_stdout(report->max_image_bytes);
    (void)fputs(",\"max_payload_bytes\":", stdout);
    plainsight_info_write_u64_stdout((uint64_t)PLAINSIGHT_MAX_PAYLOAD_BYTES);
    (void)fputs("},\"computed\":{", stdout);
    (void)fputs("\"usable_cover_bytes\":", stdout);
    plainsight_info_write_u64_stdout(report->capacity.usable_cover_bytes);
    (void)fputs(",\"usable_carrier_bits\":", stdout);
    plainsight_info_write_u64_stdout(report->capacity.usable_carrier_bits);
    (void)fputs(",\"overhead_bytes\":", stdout);
    plainsight_info_write_u64_stdout(report->capacity.overhead_bytes);
    (void)fputs(",\"max_payload_by_cover_bytes\":", stdout);
    plainsight_info_write_u64_stdout(report->capacity.max_payload_by_cover_bytes);
    (void)fputs("},\"payload\":{", stdout);
    (void)fputs("\"provided\":", stdout);
    plainsight_info_write_bool_stdout(report->payload_provided);
    (void)fputs(",\"payload_bytes\":", stdout);
    plainsight_info_write_u64_stdout(report->payload_bytes);
    (void)fputs(",\"fits_single\":", stdout);
    plainsight_info_write_bool_stdout(report->fits_single);
    (void)fputs(",\"required_shards\":", stdout);
    plainsight_info_write_u64_stdout(report->required_shards);
    (void)fputs(",\"limiting_factor\":\"", stdout);
    (void)fputs(limiting_factor_text, stdout);
    (void)fputs("\"},\"plan\":{", stdout);
    (void)fputs("\"mode\":\"", stdout);
    (void)fputs(report->plan_mode_split != 0 ? "split" : "single", stdout);
    (void)fputs("\",\"required_shards\":", stdout);
    plainsight_info_write_u64_stdout(report->required_shards);
    (void)fputs(",\"max_payload_single\":", stdout);
    plainsight_info_write_u64_stdout(report->max_payload_per_shard_bytes);
    (void)fputs(",\"per_shard_payload_estimate\":", stdout);
    plainsight_info_write_u64_stdout(report->per_shard_payload_estimate);
    (void)fputs(",\"limiting_factor\":\"", stdout);
    (void)fputs(limiting_factor_text, stdout);
    (void)fputs("\",\"output_cap_risk\":", stdout);
    plainsight_info_write_bool_stdout(report->output_cap_risk);
    (void)fputs("}}\n", stdout);
    return PLAINSIGHT_OK;
}
