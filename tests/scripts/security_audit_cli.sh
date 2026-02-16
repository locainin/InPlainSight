#!/usr/bin/env bash
set -euo pipefail

# Backwards-compatible wrapper for the CLI security audit
# The real implementation lives under tests/scripts/cli_audit for readability

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

bash "${SCRIPT_DIR}/cli_audit/run.sh"

