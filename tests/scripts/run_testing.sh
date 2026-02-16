#!/usr/bin/env bash
set -euo pipefail

# Test runner entrypoint for this repo
# Keeps scripts organized under tests/scripts and avoids writing artifacts into the working tree

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/../.." && pwd)"

run_step() {
    local step_name="$1"
    shift

    printf '%s\n' "[step] ${step_name}"
    "$@"
}

main() {
    cd "$PROJECT_ROOT"

    # Full regression (C + Rust) with a clean start
    run_step "verify (clean start)" make verify-all-clean

    # CLI audit harness (standalone, tempdir-only)
    run_step "cli security audit" bash tests/scripts/cli_audit/run.sh

    # Leave the working tree clean and small after running
    run_step "cleanup build artifacts" make clean-all

    printf '%s\n' "[ok] all checks passed"
}

main "$@"
