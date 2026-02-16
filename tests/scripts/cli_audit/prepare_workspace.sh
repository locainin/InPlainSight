#!/usr/bin/env bash
set -euo pipefail

# Workspace/fixtures for CLI audit scripts
# This file is sourced by other scripts and is not meant to be executed directly

audit_prepare_workspace() {
    AUDIT_WORK_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/inplainsight_security_audit.XXXXXX")"
    AUDIT_REPORT_PATH="${AUDIT_WORK_ROOT}/security_audit_report.txt"

    # Work dir is deleted by default so repeated runs do not leave files behind
    # Set KEEP_AUDIT_WORK=1 to keep the directory for debugging
    if [ "${KEEP_AUDIT_WORK:-0}" != "1" ]; then
        trap 'rm -rf "$AUDIT_WORK_ROOT"' EXIT
    else
        trap 'printf "%s\n" "[info] kept audit workspace: ${AUDIT_WORK_ROOT}"' EXIT
    fi

    : >"$AUDIT_REPORT_PATH"

    # Marker payload helps catch accidental plaintext leaks in output bytes
    printf 'AUDIT_MARKER_LINE_001\nAUDIT_MARKER_LINE_002\n' >"${AUDIT_WORK_ROOT}/payload.txt"
    printf 'this-is-a-strong-audit-passphrase\n' >"${AUDIT_WORK_ROOT}/passphrase.txt"
    printf 'this-is-the-wrong-passphrase\n' >"${AUDIT_WORK_ROOT}/wrong_passphrase.txt"
    printf '\n' >"${AUDIT_WORK_ROOT}/empty_passphrase.txt"
    head -c 300 /dev/zero >"${AUDIT_WORK_ROOT}/tiny.bin"

    # Minimal valid PDF payload for binary roundtrip checks
    cat >"${AUDIT_WORK_ROOT}/payload.pdf" <<'PDFEOF'
%PDF-1.4
1 0 obj
<< /Type /Catalog /Pages 2 0 R >>
endobj
2 0 obj
<< /Type /Pages /Count 1 /Kids [3 0 R] >>
endobj
3 0 obj
<< /Type /Page /Parent 2 0 R /MediaBox [0 0 200 200] /Contents 4 0 R >>
endobj
4 0 obj
<< /Length 44 >>
stream
BT /F1 12 Tf 40 100 Td (InPlainSight PDF test) Tj ET
endstream
endobj
xref
0 5
0000000000 65535 f
0000000009 00000 n
0000000058 00000 n
0000000115 00000 n
0000000202 00000 n
trailer
<< /Size 5 /Root 1 0 R >>
startxref
299
%%EOF
PDFEOF

    # Build one baseline 8-bit cover, then convert to common formats
    magick -size 512x512 gradient:'#1f2b44-#d2e3ff' -depth 8 -colorspace sRGB "${AUDIT_WORK_ROOT}/cover.png"
    magick "${AUDIT_WORK_ROOT}/cover.png" "${AUDIT_WORK_ROOT}/cover.bmp"
    magick "${AUDIT_WORK_ROOT}/cover.png" -quality 92 "${AUDIT_WORK_ROOT}/cover.jpg"
    magick "${AUDIT_WORK_ROOT}/cover.png" "${AUDIT_WORK_ROOT}/cover.webp"
    cjxl "${AUDIT_WORK_ROOT}/cover.png" "${AUDIT_WORK_ROOT}/cover.jxl" --quiet

    # PPM backend expects binary P6 bytes, so emit a known-good P6 sample directly
    {
        printf 'P6\n512 512\n255\n'
        head -c $((512 * 512 * 3)) /dev/zero
    } >"${AUDIT_WORK_ROOT}/cover.ppm"
}
