// InPlainSight C module
// Single-image hide report output stays line-based for the UI log panel

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "headers/single.h"

void plainsight_cli_hide_write_u64_stderr(uint64_t value) {
  uint8_t reversed_digits[32];
  size_t digit_count = 0u;
  size_t output_index = 0u;

  if (value == 0u) {
    (void)fputc('0', stderr);
    return;
  }

  // Build digits in reverse order then write forward for decimal output
  while (value > 0u && digit_count < sizeof(reversed_digits)) {
    reversed_digits[digit_count] = (uint8_t)('0' + (uint8_t)(value % 10u));
    value /= 10u;
    digit_count++;
  }

  while (output_index < digit_count) {
    const size_t reverse_index = digit_count - 1u - output_index;
    (void)fputc((int)reversed_digits[reverse_index], stderr);
    output_index++;
  }
}

void plainsight_cli_hide_log_size_limits(uint64_t payload_file_size, uint64_t max_payload_by_cover) {
  // Keep preflight output line-based so UI log panel can display exact numbers
  (void)fputs("hide preflight failed: payload exceeds safe limits\n", stderr);
  (void)fputs("  payload bytes: ", stderr);
  plainsight_cli_hide_write_u64_stderr(payload_file_size);
  (void)fputc('\n', stderr);
  (void)fputs("  max payload by selected cover: ", stderr);
  plainsight_cli_hide_write_u64_stderr(max_payload_by_cover);
  (void)fputc('\n', stderr);
  (void)fputs("  project payload cap: ", stderr);
  plainsight_cli_hide_write_u64_stderr((uint64_t)PLAINSIGHT_MAX_SINGLE_PAYLOAD_BYTES);
  (void)fputs(" bytes\n", stderr);
  (void)fputs(
      "  hint: payload exceeds this cover; use split mode or choose/prepare a larger lossless cover\n",
      stderr);
}

void plainsight_cli_hide_log_preflight(const plainsight_info_report *report,
                                       const plainsight_cli_hide_preflight_log_input *input) {
  uint64_t original_payload_bytes = 0u;
  uint64_t effective_payload_bytes = 0u;
  uint8_t compression_mode = 0u;

  if (report == NULL || input == NULL) {
    return;
  }
  original_payload_bytes = input->original_payload_bytes;
  effective_payload_bytes = input->effective_payload_bytes;
  compression_mode = input->compression_mode;

  (void)fputs("hide preflight report\n", stderr);
  (void)fputs("  plan: single\n", stderr);
  (void)fputs("  payload bytes: ", stderr);
  plainsight_cli_hide_write_u64_stderr(original_payload_bytes);
  (void)fputc('\n', stderr);
  (void)fputs("  payload bytes after compression: ", stderr);
  plainsight_cli_hide_write_u64_stderr(effective_payload_bytes);
  (void)fputc('\n', stderr);
  (void)fputs("  compression: ", stderr);
  if (compression_mode == PLAINSIGHT_COMPRESSION_ZSTD) {
    (void)fputs("zstd\n", stderr);
  } else {
    (void)fputs("none\n", stderr);
  }
  (void)fputs("  max payload by selected cover: ", stderr);
  plainsight_cli_hide_write_u64_stderr(report->capacity.max_payload_by_cover_bytes);
  (void)fputc('\n', stderr);
  (void)fputs("  required shards: ", stderr);
  plainsight_cli_hide_write_u64_stderr(report->required_shards);
  (void)fputc('\n', stderr);
}
