#ifndef PLAINSIGHT_CLI_PARSE_VALUES_H
#define PLAINSIGHT_CLI_PARSE_VALUES_H

#include <stdint.h>

#include "../../../../include/error.h"

// Strict unsigned integer parser shared by command option parsers
plainsight_error plainsight_cli_parse_u8_arg(const char *value_text, uint8_t minimum_value,
                                             uint8_t maximum_value, uint8_t *output_value);

// Used for payload-byte planning input where overflow must fail closed
plainsight_error plainsight_cli_parse_u64_arg(const char *value_text, uint64_t *output_value);

// Density is stored as per-mille so float text never reaches core capacity math
plainsight_error plainsight_cli_parse_density_arg(const char *value_text, uint16_t *density_per_mille);

// Duplicate flags are rejected instead of relying on last-value-wins behavior
plainsight_error plainsight_cli_reject_duplicate_arg(const char *flag_text);

#endif
