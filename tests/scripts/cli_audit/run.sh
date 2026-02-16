#!/usr/bin/env bash
set -euo pipefail

# CLI security audit entrypoint
# Runs focused audit parts and keeps all output under a temp directory

SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd -- "${SCRIPT_DIR}/../../.." && pwd)"

# shellcheck disable=SC1091
# shellcheck source=tests/scripts/cli_audit/audit_lib.sh
. "${SCRIPT_DIR}/audit_lib.sh"

# shellcheck disable=SC1091
# shellcheck source=tests/scripts/cli_audit/prepare_workspace.sh
. "${SCRIPT_DIR}/prepare_workspace.sh"

# shellcheck disable=SC1091
# shellcheck source=tests/scripts/cli_audit/test_info.sh
. "${SCRIPT_DIR}/test_info.sh"

# shellcheck disable=SC1091
# shellcheck source=tests/scripts/cli_audit/test_roundtrip.sh
. "${SCRIPT_DIR}/test_roundtrip.sh"

# shellcheck disable=SC1091
# shellcheck source=tests/scripts/cli_audit/test_pdf.sh
. "${SCRIPT_DIR}/test_pdf.sh"

main() {
    # shellcheck disable=SC2034
    AUDIT_BINARY_PATH="${PROJECT_ROOT}/inplainsight"
    # shellcheck disable=SC2034
    AUDIT_PASS_COUNT=0
    # shellcheck disable=SC2034
    AUDIT_FAIL_COUNT=0
    # shellcheck disable=SC2034
    AUDIT_WORK_ROOT=""
    # shellcheck disable=SC2034
    AUDIT_REPORT_PATH=""

    audit_require_tool jq
    audit_require_tool rg
    audit_require_tool strings
    audit_require_tool dd
    audit_require_tool magick
    audit_require_tool cjxl

    # Print once (tee to /dev/stderr would duplicate the line in terminals that show both streams)
    printf '%s\n' "[phase] cli security audit start" >&2

    audit_prepare_workspace

    if [ ! -x "$AUDIT_BINARY_PATH" ]; then
        audit_log_line "[fatal] expected CLI binary at ${AUDIT_BINARY_PATH}"
        audit_log_line "[hint] run: make gcc-sanitize"
        exit 1
    fi

    audit_test_info_matrix
    audit_test_hide_extract_roundtrip
    audit_test_pdf_roundtrip

    audit_log_line "[summary] pass=${AUDIT_PASS_COUNT} fail=${AUDIT_FAIL_COUNT}"
    audit_log_line "[report] ${AUDIT_REPORT_PATH}"

    if [ "$AUDIT_FAIL_COUNT" -ne 0 ]; then
        exit 1
    fi
}

main "$@"
