#ifndef PLAINSIGHT_CLI_H
#define PLAINSIGHT_CLI_H

#ifdef __cplusplus
extern "C" {
#endif

// Entry point for command parsing and program execution
// Returns 0 on success, 1 on runtime failure, 2 on usage/argument failures
int plainsight_cli_run(int argc, char **argv);

#ifdef __cplusplus
}
#endif

#endif
