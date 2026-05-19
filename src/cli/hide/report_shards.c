// InPlainSight C module
// Split hide status logging stays small and does not own shard write logic

#include <stdint.h>
#include <stdio.h>

#include "headers/shards.h"

static void plainsight_cli_hide_shard_write_u64_stderr(uint64_t value) {
  uint8_t reversed_digits[32];
  size_t digit_count = 0u;
  size_t output_index = 0u;

  if (value == 0u) {
    (void)fputc('0', stderr);
    return;
  }

  // Digits are staged in reverse so decimal output avoids locale-sensitive formatting
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

void plainsight_cli_hide_shard_log_preflight(const plainsight_info_report *report, const char *template_text,
                                             uint64_t payload_file_size) {
  if (report == NULL || template_text == NULL) {
    return;
  }

  (void)fputs("hide preflight report\n", stderr);
  (void)fputs("  plan: split\n", stderr);
  (void)fputs("  payload bytes: ", stderr);
  plainsight_cli_hide_shard_write_u64_stderr(payload_file_size);
  (void)fputc('\n', stderr);
  (void)fputs("  max payload per shard: ", stderr);
  plainsight_cli_hide_shard_write_u64_stderr(report->max_payload_per_shard_bytes);
  (void)fputc('\n', stderr);
  (void)fputs("  required shards: ", stderr);
  plainsight_cli_hide_shard_write_u64_stderr(report->required_shards);
  (void)fputc('\n', stderr);
  (void)fputs("  resolved output template: ", stderr);
  (void)fputs(template_text, stderr);
  (void)fputc('\n', stderr);
}
