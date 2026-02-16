// User-facing strings used in hide dialogs and logs
//
// These helpers keep long string building out of the control flow code

use crate::app::app_logging::{LogLevel, append_structured_log};

// Bytes are reported by the C CLI, but users usually reason in MiB
pub(super) fn format_bytes_for_display(byte_count: u64) -> String {
    // Floating point is used here only for display
    // All planning and limits remain integer based
    let mib_value = (byte_count as f64) / (1024.0 * 1024.0);
    format!("{} bytes ({:.2} MiB)", byte_count, mib_value)
}

// Snippets are used for debugging unexpected CLI output without flooding logs
// max_bytes is a best-effort bound on the returned UTF-8 string length
pub(super) fn build_bounded_snippet(full_text: &str, max_bytes: usize) -> String {
    // A zero limit is treated as "show nothing"
    if max_bytes == 0 {
        return String::new();
    }

    // Small strings are returned as-is to keep log output readable
    if full_text.len() <= max_bytes {
        return full_text.to_string();
    }

    // Use character iteration to avoid splitting inside a UTF-8 codepoint
    let head_chars = max_bytes / 2;
    let tail_chars = max_bytes - head_chars;

    // Head and tail are shown so errors near the end are still visible
    let head_text: String = full_text.chars().take(head_chars).collect();
    let tail_text: String = full_text
        .chars()
        .rev()
        .take(tail_chars)
        .collect::<Vec<char>>()
        .into_iter()
        .rev()
        .collect();

    format!("{}\n...\n{}", head_text, tail_text)
}

pub(super) fn build_split_warning_detail(
    preflight_plan: &crate::app::app_execution::planning::HidePreflightPlan,
    output_dir: &std::path::Path,
) -> String {
    // The warning text is intentionally explicit so the user understands why multiple images are produced
    let mut detail_lines: Vec<String> = Vec::new();

    // Cover details explain why capacity is limited for small images
    detail_lines.push(format!(
        "Cover image: {} ({}x{}, {} channels, {} decoded bytes)",
        preflight_plan.cover_format,
        preflight_plan.cover_width,
        preflight_plan.cover_height,
        preflight_plan.cover_channels,
        preflight_plan.cover_decoded_bytes
    ));
    detail_lines.push(format!(
        "Payload size: {}",
        format_bytes_for_display(preflight_plan.payload_bytes)
    ));
    // Capacity is based on the selected embedding method and cover pixel data
    detail_lines.push(format!(
        "Estimated single-image capacity (current cover): {}",
        format_bytes_for_display(preflight_plan.max_payload_by_cover_bytes)
    ));
    detail_lines.push(format!(
        "Max payload per shard (planner cap): {}",
        format_bytes_for_display(preflight_plan.max_payload_per_shard)
    ));
    detail_lines.push(format!(
        "This operation will generate {} shard images",
        preflight_plan.required_shards
    ));
    detail_lines.push(format!("Output folder: {}", output_dir.to_string_lossy()));

    // Provide a rough size suggestion when the cover is the limiting factor
    // This is intentionally phrased as a rough estimate, not a guarantee
    if preflight_plan.max_payload_by_cover_bytes > 0
        && preflight_plan.limiting_factor == "cover_capacity"
        && preflight_plan.cover_width > 0
        && preflight_plan.cover_height > 0
    {
        let scale_factor = (preflight_plan.payload_bytes as f64)
            / (preflight_plan.max_payload_by_cover_bytes as f64);
        if scale_factor > 1.0 {
            // Pixel capacity scales with area, so a square root gives a rough linear dimension factor
            let linear_scale = scale_factor.sqrt();
            let suggested_width =
                ((preflight_plan.cover_width as f64) * linear_scale).ceil() as u32;
            let suggested_height =
                ((preflight_plan.cover_height as f64) * linear_scale).ceil() as u32;
            detail_lines.push(String::new());
            detail_lines.push("Suggestion to avoid splitting:".to_string());
            detail_lines.push(format!(
                "Use a larger cover image (rough target: {}x{} pixels or larger)",
                suggested_width, suggested_height
            ));
        }
    } else if preflight_plan.limiting_factor != "cover_capacity" {
        // Some limits are not cover dependent, like project caps
        detail_lines.push(String::new());
        detail_lines.push(format!(
            "Note: limiting factor is '{}', so a larger cover may not avoid splitting",
            preflight_plan.limiting_factor
        ));
    }

    if preflight_plan.plan_output_cap_risk {
        // Encoded output size depends on the image codec and content
        // This warns about format-specific file size caps in the CLI
        detail_lines.push(String::new());
        detail_lines.push(
            "Note: output size is content-dependent and may hit configured output file caps"
                .to_string(),
        );
    }

    detail_lines.join("\n")
}

pub(super) fn log_stdout_snippet_for_json_parse_failure(
    log_buffer: &gtk4::TextBuffer,
    stdout_text: &str,
) {
    // Empty stdout is common for clean failures, so avoid logging an empty snippet label
    let stdout_trimmed = stdout_text.trim();
    if stdout_trimmed.is_empty() {
        return;
    }

    // Snippet size is bounded so logs remain usable in long sessions
    append_structured_log(
        log_buffer,
        "hide",
        LogLevel::Error,
        "captured stdout snippet",
    );
    let snippet_text = build_bounded_snippet(stdout_trimmed, 480);
    for line_text in snippet_text.lines() {
        append_structured_log(log_buffer, "hide", LogLevel::Error, line_text);
    }
}
