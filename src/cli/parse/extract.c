// InPlainSight C module
// Extract argument parsing keeps single-image and split-folder selection explicit

#include <stdio.h>
#include <string.h>

#include "../internal.h"
#include "headers/values.h"

plainsight_error plainsight_cli_parse_extract_args(int argc, char **argv,
                                                   plainsight_extract_options *options) {
  int argument_index = 0;
  int method_seen = 0;

  if (argv == NULL || options == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  options->input_path = NULL;
  options->input_dir = NULL;
  options->output_path = NULL;
  options->passphrase_path = NULL;
  options->method = PLAINSIGHT_EMBED_LSB;

  for (argument_index = 2; argument_index < argc; argument_index++) {
    const char *argument_text = argv[argument_index];

    if ((strcmp(argument_text, "--input") == 0) && (argument_index + 1 < argc)) {
      if (options->input_path != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->input_path = argv[argument_index];
      continue;
    }
    if ((strcmp(argument_text, "--input-dir") == 0) && (argument_index + 1 < argc)) {
      if (options->input_dir != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->input_dir = argv[argument_index];
      continue;
    }

    if ((strcmp(argument_text, "--output") == 0) && (argument_index + 1 < argc)) {
      if (options->output_path != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->output_path = argv[argument_index];
      continue;
    }

    if ((strcmp(argument_text, "--passphrase-file") == 0) && (argument_index + 1 < argc)) {
      if (options->passphrase_path != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->passphrase_path = argv[argument_index];
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
        (void)fputs("invalid --method for extract command\n", stderr);
        return result_code;
      }
      method_seen = 1;
      continue;
    }

    (void)fputs("unknown or incomplete extract argument: ", stderr);
    (void)fputs(argument_text, stderr);
    (void)fputs("\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }

  if ((options->input_path == NULL && options->input_dir == NULL) ||
      (options->input_path != NULL && options->input_dir != NULL)) {
    (void)fputs("extract requires exactly one of --input or --input-dir\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->output_path == NULL) {
    (void)fputs("missing required --output for extract command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->passphrase_path == NULL) {
    (void)fputs("missing required --passphrase-file for extract command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }

  return PLAINSIGHT_OK;
}
