#!/usr/bin/env bash
set -euo pipefail

# Hide/extract/tamper roundtrip coverage for CLI audit scripts
# This file is sourced by other scripts and is not meant to be executed directly

audit_test_hide_extract_roundtrip() {
    local cover_path=""
    local cover_stem=""
    local stego_path=""
    local extracted_path=""
    local tampered_path=""
    local file_size=0
    local seek_offset=0

    for cover_path in \
        "${AUDIT_WORK_ROOT}/cover.png" \
        "${AUDIT_WORK_ROOT}/cover.jxl" \
        "${AUDIT_WORK_ROOT}/cover.bmp" \
        "${AUDIT_WORK_ROOT}/cover.ppm" \
        "${AUDIT_WORK_ROOT}/cover.jpg" \
        "${AUDIT_WORK_ROOT}/cover.webp"; do
        cover_stem="$(basename "$cover_path")"
        stego_path="${AUDIT_WORK_ROOT}/stego_${cover_stem}.png"
        extracted_path="${AUDIT_WORK_ROOT}/extracted_${cover_stem}.bin"
        tampered_path="${AUDIT_WORK_ROOT}/tampered_${cover_stem}.png"

        if ! audit_run_expect_success "hide roundtrip setup for ${cover_stem}" \
            "$AUDIT_BINARY_PATH" hide --cover "$cover_path" --payload "${AUDIT_WORK_ROOT}/payload.txt" \
            --output "$stego_path" --passphrase-file "${AUDIT_WORK_ROOT}/passphrase.txt" --method lsb; then
            continue
        fi

        if ! audit_run_expect_success "extract roundtrip for ${cover_stem}" \
            "$AUDIT_BINARY_PATH" extract --input "$stego_path" --output "$extracted_path" \
            --passphrase-file "${AUDIT_WORK_ROOT}/passphrase.txt" --method lsb; then
            continue
        fi

        if cmp -s "${AUDIT_WORK_ROOT}/payload.txt" "$extracted_path"; then
            audit_mark_pass "payload bytes match after extract for ${cover_stem}"
        else
            audit_mark_fail "payload bytes mismatch after extract for ${cover_stem}"
        fi

        audit_run_expect_failure "wrong passphrase fails for ${cover_stem}" \
            "$AUDIT_BINARY_PATH" extract --input "$stego_path" --output "${extracted_path}.wrong" \
            --passphrase-file "${AUDIT_WORK_ROOT}/wrong_passphrase.txt" --method lsb

        cp "$stego_path" "$tampered_path"
        file_size="$(stat -c '%s' "$tampered_path")"
        seek_offset=$((file_size / 2))
        dd if=/dev/urandom of="$tampered_path" bs=1 count=1 seek="$seek_offset" conv=notrunc status=none

        audit_run_expect_failure "tampered stego fails auth for ${cover_stem}" \
            "$AUDIT_BINARY_PATH" extract --input "$tampered_path" --output "${extracted_path}.tampered" \
            --passphrase-file "${AUDIT_WORK_ROOT}/passphrase.txt" --method lsb

        if strings "$stego_path" | rg -F "AUDIT_MARKER_LINE_001" >/dev/null; then
            audit_mark_fail "plaintext marker leaked in output bytes for ${cover_stem}"
        else
            audit_mark_pass "no plaintext marker leak for ${cover_stem}"
        fi
    done
}

