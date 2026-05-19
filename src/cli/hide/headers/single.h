#ifndef PLAINSIGHT_CLI_HIDE_HEADERS_SINGLE_H
#define PLAINSIGHT_CLI_HIDE_HEADERS_SINGLE_H

#include <stddef.h>
#include <stdint.h>

#include "../../internal.h"

void plainsight_cli_hide_write_u64_stderr(uint64_t value);

void plainsight_cli_hide_log_size_limits(uint64_t payload_file_size, uint64_t max_payload_by_cover);

void plainsight_cli_hide_log_preflight(const plainsight_info_report *report, uint64_t original_payload_bytes,
                                       uint64_t effective_payload_bytes, uint8_t compression_mode);

plainsight_error plainsight_cli_hide_choose_auto_compression(size_t payload_length,
                                                             size_t *compressed_length_out);

#endif
