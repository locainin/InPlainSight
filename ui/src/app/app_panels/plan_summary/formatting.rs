pub(super) fn basename_or_dash(path_text: &str) -> String {
    if path_text.trim().is_empty() {
        return "-".to_string();
    }

    std::path::Path::new(path_text)
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or(path_text)
        .to_string()
}

pub(super) fn payload_summary(path_text: &str) -> String {
    if path_text.trim().is_empty() {
        return "-".to_string();
    }

    let path_value = std::path::Path::new(path_text);
    let name = path_value
        .file_name()
        .and_then(|value| value.to_str())
        .unwrap_or(path_text);
    if let Ok(metadata) = std::fs::metadata(path_value) {
        return format!("{} ({})", name, format_file_size(metadata.len()));
    }

    name.to_string()
}

fn format_file_size(byte_count: u64) -> String {
    const KIB: u64 = 1024;
    const MIB: u64 = KIB * 1024;
    const GIB: u64 = MIB * 1024;

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
    let whole = byte_count / unit_size;
    let decimal = ((byte_count % unit_size) * 10) / unit_size;
    format!("{whole}.{decimal} {unit_label}")
}
