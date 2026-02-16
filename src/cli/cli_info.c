#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "../../include/info.h"
#include "cli_internal.h"

#define PLAINSIGHT_MIME_OCTET_STREAM_LEN 24u

static const char *plainsight_cli_info_limiting_factor(const plainsight_info_report *report) {
    // Limiting labels are stable so UI logic can map them to user-facing hints
    if (report->payload_provided == 0) {
        return "none";
    }
    if (report->fits_single != 0) {
        return "none";
    }
    if (report->max_payload_per_shard_bytes == 0u) {
        return "cover_overhead";
    }
    if (report->payload_bytes > report->max_total_payload_bytes) {
        return "total_cap";
    }
    if (report->required_shards > PLAINSIGHT_MAX_SHARDS) {
        return "shard_count_cap";
    }
    if (report->max_payload_per_shard_bytes == (uint64_t)PLAINSIGHT_MAX_PAYLOAD_BYTES &&
        report->capacity.max_payload_by_cover_bytes >= (uint64_t)PLAINSIGHT_MAX_PAYLOAD_BYTES) {
        return "project_payload_cap";
    }
    return "cover_capacity";
}

static plainsight_error plainsight_cli_info_payload_metadata(const plainsight_info_options *options,
                                             size_t *payload_name_length,
                                             int *payload_provided,
                                             uint64_t *payload_bytes) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    size_t copied_name_length = 0u;

    if (options == NULL || payload_name_length == NULL || payload_provided == NULL ||
        payload_bytes == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }

    *payload_name_length = 0u;
    *payload_provided = 0;
    *payload_bytes = 0u;

    if (options->payload_bytes_provided != 0) {
        *payload_name_length = 0u;
        *payload_provided = 1;
        *payload_bytes = options->payload_bytes_value;
        return PLAINSIGHT_OK;
    }

    if (options->payload_path == NULL) {
        // Info command also supports cover-only planning with no payload file
        return PLAINSIGHT_OK;
    }

    // Payload basename contributes to encrypted inner metadata overhead
    result_code = plainsight_io_copy_basename(options->payload_path,
                                      g_cli_workspace.payload_name,
                                      sizeof(g_cli_workspace.payload_name),
                                      &copied_name_length);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // File-size probe avoids loading payload bytes during planning
    result_code = plainsight_io_get_regular_file_size(options->payload_path, payload_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    *payload_name_length = copied_name_length;
    *payload_provided = 1;
    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_run_info(const plainsight_info_options *options) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    plainsight_info_report info_report;
    plainsight_image_format detected_format = PLAINSIGHT_IMAGE_FORMAT_UNKNOWN;
    size_t payload_name_length = 0u;
    int payload_provided = 0;
    uint64_t payload_bytes = 0u;
    const char *limiting_factor_text = NULL;

    if (options == NULL) {
        return PLAINSIGHT_ERR_ARGS;
    }
    if (options->method != PLAINSIGHT_EMBED_LSB) {
        return PLAINSIGHT_ERR_UNSUPPORTED;
    }

    // Decode cover through normal image backend so planner sees real decoded byte length
    detected_format = plainsight_image_detect_format_from_path(options->cover_path);
    result_code = plainsight_cli_load_image(options->cover_path, &g_cli_workspace.image);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    result_code = plainsight_cli_info_payload_metadata(
        options, &payload_name_length, &payload_provided, &payload_bytes);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    // Build one canonical report that both CLI users and UI can consume
    result_code = plainsight_info_build_report(&g_cli_workspace.image,
                                       detected_format,
                                       options->lsb_bits,
                                       options->density_per_mille,
                                       payload_name_length,
                                       PLAINSIGHT_MIME_OCTET_STREAM_LEN,
                                       payload_provided,
                                       payload_bytes,
                                       &info_report);
    if (result_code != PLAINSIGHT_OK) {
        return result_code;
    }

    limiting_factor_text = plainsight_cli_info_limiting_factor(&info_report);

    if (options->json_output != 0) {
        // JSON mode is the supported machine-readable contract for automation
        return plainsight_info_write_json_stdout(&info_report, limiting_factor_text);
    }

    // Text mode keeps a concise fallback for manual terminal inspection
    (void)fputs("info command currently requires --json for stable machine output\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
}
