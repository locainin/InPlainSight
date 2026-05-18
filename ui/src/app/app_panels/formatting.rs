// Format byte counts for compact UI labels
pub(super) fn format_file_size(byte_count: u64) -> String {
    // Binary units match file-system reporting while labels keep familiar names
    const KIB: u64 = 1024;
    const MIB: u64 = KIB * 1024;
    const GIB: u64 = MIB * 1024;

    // Use one decimal for larger units so cards remain readable
    if byte_count >= GIB {
        format_decimal_unit(byte_count, GIB, "GB")
    } else if byte_count >= MIB {
        format_decimal_unit(byte_count, MIB, "MB")
    } else if byte_count >= KIB {
        format_decimal_unit(byte_count, KIB, "KB")
    } else {
        format!("{byte_count} B")
    }
}

fn format_decimal_unit(byte_count: u64, unit_size: u64, unit_label: &str) -> String {
    // Integer math avoids locale and floating-point formatting surprises
    let whole = byte_count / unit_size;
    let decimal = ((byte_count % unit_size) * 10) / unit_size;
    format!("{whole}.{decimal} {unit_label}")
}
