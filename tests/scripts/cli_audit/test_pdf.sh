#!/usr/bin/env bash
set -euo pipefail

# PDF roundtrip coverage for CLI audit scripts
# This file is sourced by other scripts and is not meant to be executed directly

audit_test_pdf_roundtrip() {
    local stego_pdf="${AUDIT_WORK_ROOT}/stego_pdf.png"
    local recovered_pdf="${AUDIT_WORK_ROOT}/recovered_payload.pdf"

    audit_run_expect_success "hide pdf payload" \
        "$AUDIT_BINARY_PATH" hide --cover "${AUDIT_WORK_ROOT}/cover.png" --payload "${AUDIT_WORK_ROOT}/payload.pdf" \
        --output "$stego_pdf" --passphrase-file "${AUDIT_WORK_ROOT}/passphrase.txt" --method lsb

    audit_run_expect_success "extract pdf payload" \
        "$AUDIT_BINARY_PATH" extract --input "$stego_pdf" --output "$recovered_pdf" \
        --passphrase-file "${AUDIT_WORK_ROOT}/passphrase.txt" --method lsb

    if cmp -s "${AUDIT_WORK_ROOT}/payload.pdf" "$recovered_pdf"; then
        audit_mark_pass "pdf payload restored exactly"
    else
        audit_mark_fail "pdf payload mismatch after extract"
    fi
}

