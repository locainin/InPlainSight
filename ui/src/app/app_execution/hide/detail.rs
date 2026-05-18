// User-facing strings used in hide dialogs and logs
//
// These helpers keep long string building out of the control flow code

use crate::app::app_logging::{LogLevel, append_structured_log};

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

    format!("{head_text}\n...\n{tail_text}")
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
