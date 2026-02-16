#include <stdio.h>
#include <string.h>

#include "../../include/cli.h"
#include "cli_internal.h"

static int plainsight_cli_argument_is_help(const char *argument_text) {
    if (argument_text == NULL) {
        return 0;
    }
    if (strcmp(argument_text, "--help") == 0) {
        return 1;
    }
    if (strcmp(argument_text, "-h") == 0) {
        return 1;
    }
    return 0;
}

static int plainsight_cli_has_help_flag(int argc, char **argv, int start_index) {
    int argument_index = 0;

    if (argv == NULL || start_index >= argc) {
        return 0;
    }

    for (argument_index = start_index; argument_index < argc; argument_index++) {
        if (plainsight_cli_argument_is_help(argv[argument_index])) {
            return 1;
        }
    }
    return 0;
}

static void plainsight_cli_print_operation_failure(const char *operation_name, plainsight_error result_code) {
    (void)fputs(operation_name, stderr);
    (void)fputs(" failed: ", stderr);
    (void)fputs(plainsight_error_str(result_code), stderr);
    (void)fputs("\n", stderr);
}

int plainsight_cli_run(int argc, char **argv) {
    plainsight_error result_code = PLAINSIGHT_ERR_INTERNAL;
    const char *program_path = NULL;

    if (argv == NULL || argc < 1) {
        plainsight_cli_print_usage(NULL);
        return 2;
    }

    program_path = argv[0];

    if (argc < 2 || argv[1] == NULL) {
        plainsight_cli_print_usage(program_path);
        return 2;
    }

    if (plainsight_cli_argument_is_help(argv[1])) {
        // Top-level help should succeed without requiring subcommands
        plainsight_cli_print_usage(program_path);
        return 0;
    }

    // Workspace init binds image pixel storage to a stable buffer
    // This prevents accidental large stack allocations and keeps image IO consistent
    plainsight_cli_workspace_init();

    result_code = plainsight_crypto_init();
    if (result_code != PLAINSIGHT_OK) {
        (void)fputs("crypto init failed\n", stderr);
        return 1;
    }

    // Keep subcommand branching flat and explicit
    if (strcmp(argv[1], "hide") == 0) {
        plainsight_hide_options hide_options = {0};

        if (plainsight_cli_has_help_flag(argc, argv, 2)) {
            // Subcommand help allows "hide --help" regardless of argument order
            plainsight_cli_print_usage(program_path);
            return 0;
        }

        result_code = plainsight_cli_parse_hide_args(argc, argv, &hide_options);
        if (result_code != PLAINSIGHT_OK) {
            (void)fputs("hide argument validation failed: ", stderr);
            (void)fputs(plainsight_error_str(result_code), stderr);
            (void)fputs("\n", stderr);
            plainsight_cli_print_usage(program_path);
            return 2;
        }

        result_code = plainsight_cli_run_hide(&hide_options);
        if (result_code != PLAINSIGHT_OK) {
            plainsight_cli_print_operation_failure("hide", result_code);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "extract") == 0) {
        plainsight_extract_options extract_options = {0};

        if (plainsight_cli_has_help_flag(argc, argv, 2)) {
            // Subcommand help allows "extract --help" regardless of argument order
            plainsight_cli_print_usage(program_path);
            return 0;
        }

        result_code = plainsight_cli_parse_extract_args(argc, argv, &extract_options);
        if (result_code != PLAINSIGHT_OK) {
            (void)fputs("extract argument validation failed: ", stderr);
            (void)fputs(plainsight_error_str(result_code), stderr);
            (void)fputs("\n", stderr);
            plainsight_cli_print_usage(program_path);
            return 2;
        }

        result_code = plainsight_cli_run_extract(&extract_options);
        if (result_code != PLAINSIGHT_OK) {
            plainsight_cli_print_operation_failure("extract", result_code);
            return 1;
        }
        return 0;
    }

    if (strcmp(argv[1], "info") == 0) {
        plainsight_info_options info_options = {0};

        if (plainsight_cli_has_help_flag(argc, argv, 2)) {
            plainsight_cli_print_usage(program_path);
            return 0;
        }

        result_code = plainsight_cli_parse_info_args(argc, argv, &info_options);
        if (result_code != PLAINSIGHT_OK) {
            (void)fputs("info argument validation failed: ", stderr);
            (void)fputs(plainsight_error_str(result_code), stderr);
            (void)fputs("\n", stderr);
            plainsight_cli_print_usage(program_path);
            return 2;
        }

        result_code = plainsight_cli_run_info(&info_options);
        if (result_code != PLAINSIGHT_OK) {
            plainsight_cli_print_operation_failure("info", result_code);
            return 1;
        }
        return 0;
    }

    (void)fputs("unknown command\n", stderr);
    plainsight_cli_print_usage(program_path);
    return 2;
}
