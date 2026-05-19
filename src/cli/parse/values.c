// InPlainSight C module
// Shared CLI value parsers keep each subcommand parser focused on its own flags

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "headers/values.h"

plainsight_error plainsight_cli_parse_u8_arg(const char *value_text, uint8_t minimum_value,
                                             uint8_t maximum_value, uint8_t *output_value) {
  char *end_ptr = NULL;
  unsigned long parsed_value = 0ul;

  if (value_text == NULL || output_value == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // strtoul is used with full end-pointer checks to reject junk suffixes
  errno = 0;
  parsed_value = strtoul(value_text, &end_ptr, 10);
  if (errno != 0 || end_ptr == value_text || end_ptr == NULL || *end_ptr != '\0') {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (parsed_value < (unsigned long)minimum_value || parsed_value > (unsigned long)maximum_value) {
    return PLAINSIGHT_ERR_ARGS;
  }

  *output_value = (uint8_t)parsed_value;
  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_parse_u64_arg(const char *value_text, uint64_t *output_value) {
  char *end_ptr = NULL;
  unsigned long long parsed_value = 0ULL;

  if (value_text == NULL || output_value == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  errno = 0;
  parsed_value = strtoull(value_text, &end_ptr, 10);
  if (errno != 0 || end_ptr == value_text || end_ptr == NULL || *end_ptr != '\0') {
    return PLAINSIGHT_ERR_ARGS;
  }

  *output_value = (uint64_t)parsed_value;
  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_parse_density_arg(const char *value_text, uint16_t *density_per_mille) {
  char *end_ptr = NULL;
  double parsed_density = 0.0;
  double scaled_density = 0.0;
  unsigned long rounded_density = 0ul;

  if (value_text == NULL || density_per_mille == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Density accepts decimal text in the range (0, 1]
  // The parser rounds to per-mille so JSON output stays integer-only
  errno = 0;
  parsed_density = strtod(value_text, &end_ptr);
  if (errno != 0 || end_ptr == value_text || end_ptr == NULL || *end_ptr != '\0') {
    return PLAINSIGHT_ERR_ARGS;
  }
  if (parsed_density <= 0.0 || parsed_density > 1.0) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // Per-mille keeps serialization deterministic while still accepting decimal input
  scaled_density = parsed_density * 1000.0;
  rounded_density = (unsigned long)(scaled_density + 0.5);
  if (rounded_density == 0ul || rounded_density > 1000ul) {
    return PLAINSIGHT_ERR_ARGS;
  }

  *density_per_mille = (uint16_t)rounded_density;
  return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_reject_duplicate_arg(const char *flag_text) {
  (void)fputs("duplicate argument: ", stderr);
  (void)fputs(flag_text, stderr);
  (void)fputc('\n', stderr);
  return PLAINSIGHT_ERR_ARGS;
}
