#!/usr/bin/env bash
set -euo pipefail

# Info/plan coverage for CLI audit scripts
# This file is sourced by other scripts and is not meant to be executed directly

audit_test_info_matrix() {
    local cover_path=""

    for cover_path in \
        "${AUDIT_WORK_ROOT}/cover.png" \
        "${AUDIT_WORK_ROOT}/cover.jxl" \
        "${AUDIT_WORK_ROOT}/cover.bmp" \
        "${AUDIT_WORK_ROOT}/cover.ppm" \
        "${AUDIT_WORK_ROOT}/cover.jpg" \
        "${AUDIT_WORK_ROOT}/cover.webp"; do
        audit_run_expect_success "info json parses for $(basename "$cover_path")" \
            bash -c "\"${AUDIT_BINARY_PATH}\" info --cover \"${cover_path}\" --payload \"${AUDIT_WORK_ROOT}/payload.txt\" --method lsb --lsb-bits 1 --density 1.0 --json | jq -e '.method.name==\"lsb\" and .payload.provided==true' >/dev/null"
    done

    audit_run_expect_failure "info rejects missing --json" \
        "$AUDIT_BINARY_PATH" info --cover "${AUDIT_WORK_ROOT}/cover.png" --method lsb

    audit_run_expect_failure "info rejects bad density string" \
        "$AUDIT_BINARY_PATH" info --cover "${AUDIT_WORK_ROOT}/cover.png" --method lsb --lsb-bits 1 --density nope --json
}

