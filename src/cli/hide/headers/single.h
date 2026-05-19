#ifndef PLAINSIGHT_CLI_HIDE_HEADERS_SINGLE_H
#define PLAINSIGHT_CLI_HIDE_HEADERS_SINGLE_H

#include <stddef.h>
#include <stdint.h>

#include "../../internal.h"

// Decimal logging is shared by preflight and error paths
// Keeping it here avoids scattered printf format choices
void plainsight_cli_hide_write_u64_stderr(uint64_t value);

// Reports capacity failure before any passphrase is read
// This keeps normal planning errors away from secret handling paths
void plainsight_cli_hide_log_size_limits(uint64_t payload_file_size, uint64_t max_payload_by_cover);

// Single-image preflight log input keeps similar byte counts named
typedef struct plainsight_cli_hide_preflight_log_input {
  // Original payload bytes before optional compression
  uint64_t original_payload_bytes;
  // Payload bytes after the selected compression mode
  uint64_t effective_payload_bytes;
  // Compression mode used for the effective payload
  uint8_t compression_mode;
} plainsight_cli_hide_preflight_log_input;

// Emits the stable CLI preflight block consumed by GUI logs
void plainsight_cli_hide_log_preflight(const plainsight_info_report *report,
                                       const plainsight_cli_hide_preflight_log_input *input);

// Tries bounded zstd levels and stores the smallest useful result in workspace ciphertext scratch
plainsight_error plainsight_cli_hide_choose_auto_compression(size_t payload_length,
                                                             size_t *compressed_length_out);

#endif
