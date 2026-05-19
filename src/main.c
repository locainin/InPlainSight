// InPlainSight C module
// Keep memory bounded: no heap allocation, explicit lengths, and checked cleanup paths

#include "../include/cli.h"

int main(int argc, char **argv) {
    // Keep process entry tiny and delegate all behavior to CLI module
    // Centralizing logic in plainsight_cli_run keeps startup code easy to audit
    return plainsight_cli_run(argc, argv);
}
