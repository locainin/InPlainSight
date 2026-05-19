// InPlainSight C module
// Hide argument parsing owns only hide-specific flags and cross-flag checks

#include <stdio.h>
#include <string.h>

#include "../internal.h"
#include "headers/values.h"

plainsight_error plainsight_cli_parse_hide_args(int argc, char **argv, plainsight_hide_options *options) {
  int argument_index = 0;
  int method_seen = 0;
  int compress_seen = 0;
  int split_seen = 0;

  if (argv == NULL || options == NULL) {
    return PLAINSIGHT_ERR_ARGS;
  }

  // options is always reset so callers never run with partial prior state
  options->cover_path = NULL;
  options->payload_path = NULL;
  options->output_path = NULL;
  options->output_dir = NULL;
  options->output_template = NULL;
  options->passphrase_path = NULL;
  options->method = PLAINSIGHT_EMBED_LSB;
  options->compression_mode = PLAINSIGHT_COMPRESSION_NONE;
  options->split_auto = 0;

  // Subcommand token is argv[1], options begin at argv[2]
  // Unknown flags fail early so callers do not run with partial state
  for (argument_index = 2; argument_index < argc; argument_index++) {
    const char *argument_text = argv[argument_index];

    // Each flag consumes its following value when present
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

    if ((strcmp(argument_text, "--output") == 0) && (argument_index + 1 < argc)) {
      if (options->output_path != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->output_path = argv[argument_index];
      continue;
    }
    if ((strcmp(argument_text, "--output-dir") == 0) && (argument_index + 1 < argc)) {
      if (options->output_dir != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->output_dir = argv[argument_index];
      continue;
    }
    if ((strcmp(argument_text, "--output-template") == 0) && (argument_index + 1 < argc)) {
      if (options->output_template != NULL) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      options->output_template = argv[argument_index];
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
    if ((strcmp(argument_text, "--split") == 0) && (argument_index + 1 < argc)) {
      if (split_seen != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      if (strcmp(argv[argument_index], "auto") != 0) {
        (void)fputs("invalid --split for hide command (allowed: auto)\n", stderr);
        return PLAINSIGHT_ERR_ARGS;
      }
      // split_auto switches hide into output-dir mode and shard planning
      options->split_auto = 1;
      split_seen = 1;
      continue;
    }

    if ((strcmp(argument_text, "--compress") == 0) && (argument_index + 1 < argc)) {
      if (compress_seen != 0) {
        return plainsight_cli_reject_duplicate_arg(argument_text);
      }
      argument_index++;
      if (strcmp(argv[argument_index], "none") == 0) {
        options->compression_mode = PLAINSIGHT_COMPRESSION_NONE;
        compress_seen = 1;
        continue;
      }
      if (strcmp(argv[argument_index], "auto") == 0) {
        options->compression_mode = PLAINSIGHT_CLI_COMPRESSION_AUTO;
        compress_seen = 1;
        continue;
      }
      if (strcmp(argv[argument_index], "zstd") == 0) {
        options->compression_mode = PLAINSIGHT_COMPRESSION_ZSTD;
        compress_seen = 1;
        continue;
      }
      (void)fputs("invalid --compress for hide command (allowed: none, auto, zstd)\n", stderr);
      return PLAINSIGHT_ERR_ARGS;
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
        (void)fputs("invalid --method for hide command\n", stderr);
        return result_code;
      }
      method_seen = 1;
      continue;
    }

    // Unknown flags are rejected so callers do not accidentally rely on silent fallback
    (void)fputs("unknown or incomplete hide argument: ", stderr);
    (void)fputs(argument_text, stderr);
    (void)fputs("\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }

  // Missing required flags are reported explicitly for faster CLI troubleshooting
  if (options->cover_path == NULL) {
    (void)fputs("missing required --cover for hide command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->payload_path == NULL) {
    (void)fputs("missing required --payload for hide command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->split_auto == 0) {
    if (options->output_path == NULL) {
      (void)fputs("missing required --output for hide command\n", stderr);
      return PLAINSIGHT_ERR_ARGS;
    }
  } else {
    if (options->output_dir == NULL) {
      (void)fputs("missing required --output-dir for hide split mode\n", stderr);
      return PLAINSIGHT_ERR_ARGS;
    }
  }
  if (options->passphrase_path == NULL) {
    (void)fputs("missing required --passphrase-file for hide command\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->output_template != NULL && options->split_auto == 0) {
    (void)fputs("--output-template requires --split auto\n", stderr);
    return PLAINSIGHT_ERR_ARGS;
  }
  if (options->split_auto != 0 && options->compression_mode != PLAINSIGHT_COMPRESSION_NONE) {
    (void)fputs("--compress is not supported with --split auto in this build\n", stderr);
    return PLAINSIGHT_ERR_UNSUPPORTED;
  }

  return PLAINSIGHT_OK;
}
