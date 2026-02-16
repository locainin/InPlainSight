#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "cli_internal.h"

static plainsight_error plainsight_cli_parse_u8_arg(const char *value_text,
                                    uint8_t minimum_value,
                                    uint8_t maximum_value,
                                    uint8_t *output_value) {
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

static plainsight_error plainsight_cli_parse_u64_arg(const char *value_text, uint64_t *output_value) {
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

static plainsight_error plainsight_cli_parse_density_arg(const char *value_text, uint16_t *density_per_mille) {
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

plainsight_error plainsight_cli_parse_hide_args(int argc, char **argv, plainsight_hide_options *options) {
    int argument_index = 0;

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
    options->split_auto = 0;

    // Subcommand token is argv[1], options begin at argv[2]
    // Unknown flags fail early so callers do not run with partial state
    for (argument_index = 2; argument_index < argc; argument_index++) {
        const char *argument_text = argv[argument_index];

        // Each flag consumes its following value when present
        if ((strcmp(argument_text, "--cover") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->cover_path = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--payload") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->payload_path = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--output") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->output_path = argv[argument_index];
            continue;
        }
        if ((strcmp(argument_text, "--output-dir") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->output_dir = argv[argument_index];
            continue;
        }
        if ((strcmp(argument_text, "--output-template") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->output_template = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--passphrase-file") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->passphrase_path = argv[argument_index];
            continue;
        }
        if ((strcmp(argument_text, "--split") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            if (strcmp(argv[argument_index], "auto") != 0) {
                (void)fputs("invalid --split for hide command (allowed: auto)\n", stderr);
                return PLAINSIGHT_ERR_ARGS;
            }
            // split_auto switches hide into output-dir mode and shard planning
            options->split_auto = 1;
            continue;
        }

        if ((strcmp(argument_text, "--method") == 0) && (argument_index + 1 < argc)) {
            plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
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

    return PLAINSIGHT_OK;
}

plainsight_error plainsight_cli_parse_extract_args(int argc, char **argv, plainsight_extract_options *options) {
    int argument_index = 0;

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
            argument_index++;
            options->input_path = argv[argument_index];
            continue;
        }
        if ((strcmp(argument_text, "--input-dir") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->input_dir = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--output") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->output_path = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--passphrase-file") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->passphrase_path = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--method") == 0) && (argument_index + 1 < argc)) {
            plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
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

plainsight_error plainsight_cli_parse_info_args(int argc, char **argv, plainsight_info_options *options) {
    int argument_index = 0;

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

    for (argument_index = 2; argument_index < argc; argument_index++) {
        const char *argument_text = argv[argument_index];

        if ((strcmp(argument_text, "--cover") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->cover_path = argv[argument_index];
            continue;
        }

        if ((strcmp(argument_text, "--payload") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            options->payload_path = argv[argument_index];
            continue;
        }
        if ((strcmp(argument_text, "--payload-bytes") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            if (plainsight_cli_parse_u64_arg(argv[argument_index], &options->payload_bytes_value) != PLAINSIGHT_OK) {
                (void)fputs("invalid --payload-bytes for info command\n", stderr);
                return PLAINSIGHT_ERR_ARGS;
            }
            options->payload_bytes_provided = 1;
            continue;
        }

        if ((strcmp(argument_text, "--method") == 0) && (argument_index + 1 < argc)) {
            plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
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
            continue;
        }

        if ((strcmp(argument_text, "--lsb-bits") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            if (plainsight_cli_parse_u8_arg(argv[argument_index], 1u, 1u, &options->lsb_bits) != PLAINSIGHT_OK) {
                (void)fputs("invalid --lsb-bits for info command (allowed: 1)\n", stderr);
                return PLAINSIGHT_ERR_ARGS;
            }
            continue;
        }

        if ((strcmp(argument_text, "--density") == 0) && (argument_index + 1 < argc)) {
            argument_index++;
            if (plainsight_cli_parse_density_arg(argv[argument_index], &options->density_per_mille) != PLAINSIGHT_OK) {
                (void)fputs("invalid --density for info command (range: 0.001 to 1.0)\n", stderr);
                return PLAINSIGHT_ERR_ARGS;
            }
            continue;
        }

        if (strcmp(argument_text, "--json") == 0) {
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
    if (options->payload_path != NULL && options->payload_bytes_provided != 0) {
        (void)fputs("use either --payload or --payload-bytes, not both\n", stderr);
        return PLAINSIGHT_ERR_ARGS;
    }

    return PLAINSIGHT_OK;
}
