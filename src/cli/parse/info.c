// InPlainSight C module
// Info argument parsing owns capacity and JSON planning flags

#include <stdio.h>
#include <string.h>

#include "../internal.h"
#include "headers/values.h"

plainsight_error plainsight_cli_parse_info_args(int argc, char **argv, plainsight_info_options *options) {
  int argument_index = 0;
  int method_seen = 0;
  int lsb_seen = 0;
  int density_seen = 0;

  if (argv == NULL || options == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  options->cover_path = NULL;
  options->payload_path = NULL;
  options->payload_bytes_provided = 0;
  options->payload_bytes_value = 0u;
  options->method = PLAINSIGHT_EMBED_LSB;
  options->lsb_bits = 1u;
  options->density_per_mille = 1000u;
  options->json_output = 0;

  // Info is the preflight path, so defaults must mirror hide without reading secrets
  for (argument_index = 2; argument_index < argc; argument_index++) {
    const char *argument_text = argv[argument_index];

    if ((strcmp(argument_text, "--cover") == 0) && (argument_index + 1 < argc)) {
      if (options->cover_path != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->cover_path = argv[argument_index];
      continue;
    }

    if ((strcmp(argument_text, "--payload") == 0) && (argument_index + 1 < argc)) {
      if (options->payload_path != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->payload_path = argv[argument_index];
      continue;
    }
    if ((strcmp(argument_text, "--payload-bytes") == 0) && (argument_index + 1 < argc)) {
      if (options->payload_bytes_provided != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      if (plainsight_cli_parse_u64_arg(argv[argument_index], &options->payload_bytes_value) !=
          PLAINSIGHT_OK) {
        (void)fputs("invalid --payload-bytes for info command\n", stderr);
        return PLAINSIGHT_ERR_ARGS;
      }
      options->payload_bytes_provided = 1;
      continue;
    }

    if ((strcmp(argument_text, "--method") == 0) && (argument_index + 1 < argc)) {
      plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
      if (method_seen != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->method = plainsight_cli_parse_method(argv[argument_index], &result_code);
      if (result_code != PLAINSIGHT_OK) {
        if (result_code == PLAINSIGHT_ERR_UNSUPPORTED) {
          (void)fputs("method is not supported in this build (allowed: lsb)\n", stderr);
          return result_code;
        }
        (void)fputs("invalid --method for info command\n", stderr);
        return result_code;
      }
      method_seen = 1;
      continue;
    }

    if ((strcmp(argument_text, "--lsb-bits") == 0) && (argument_index + 1 < argc)) {
      if (lsb_seen != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      if (plainsight_cli_parse_u8_arg(argv[argument_index], 1u, 1u, &options->lsb_bits) != PLAINSIGHT_OK) {
        (void)fputs("invalid --lsb-bits for info command (allowed: 1)\n", stderr);
        return PLAINSIGHT_ERR_ARGS;
      }
      lsb_seen = 1;
      continue;
    }

    if ((strcmp(argument_text, "--density") == 0) && (argument_index + 1 < argc)) {
      if (density_seen != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      if (plainsight_cli_parse_density_arg(argv[argument_index], &options->density_per_mille) !=
          PLAINSIGHT_OK) {
        (void)fputs("invalid --density for info command (range: 0.001 to 1.0)\n", stderr);
        return PLAINSIGHT_ERR_ARGS;
      }
      density_seen = 1;
      continue;
    }

    if (strcmp(argument_text, "--json") == 0) {
      // JSON is required so callers receive a stable machine-readable contract
      if (options->json_output != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      options->json_output = 1;
      continue;
    }

    (void)fputs("unknown or incomplete info argument: ", stderr);
    (void)fputs(argument_text, stderr);
    (void)fputs("\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }

  if (options->cover_path == NULL) {
    (void)fputs("missing required --cover for info command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->json_output == 0) {
    (void)fputs("missing required --json for info command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  // Payload size can come from a real file or from the GUI's already-known byte count
  if (options->payload_path != NULL && options->payload_bytes_provided != 0) {
    (void)fputs("use either --payload or --payload-bytes, not both\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }

  return PLAINSIGHT_OK;
}
