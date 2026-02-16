#!/usr/bin/env bash
set -euo pipefail

# Shared helpers for CLI audit scripts
# This file is sourced by other scripts and is not meant to be executed directly

audit_log_line() {
    printf '%s\n' "$1" | tee -a "$AUDIT_REPORT_PATH"
}

audit_mark_pass() {
    AUDIT_PASS_COUNT=$((AUDIT_PASS_COUNT + 1))
    audit_log_line "[pass] $1"
}

audit_mark_fail() {
    AUDIT_FAIL_COUNT=$((AUDIT_FAIL_COUNT + 1))
    audit_log_line "[fail] $1"
}

audit_require_tool() {
    local tool_name="$1"

    if ! command -v "$tool_name" >/dev/null 2>&1; then
        audit_log_line "[fatal] required tool missing: ${tool_name}"
        exit 1
    fi
}

audit_run_expect_success() {
    local test_name="$1"
    shift

    if "$@" >>"$AUDIT_REPORT_PATH" 2>&1; then
        audit_mark_pass "$test_name"
        return 0
    fi

    audit_mark_fail "$test_name"
    return 1
}

audit_run_expect_failure() {
    local test_name="$1"
    shift

    if "$@" >>"$AUDIT_REPORT_PATH" 2>&1; then
        audit_mark_fail "$test_name (unexpected success)"
        return 1
    fi

    audit_mark_pass "$test_name"
    return 0
}

