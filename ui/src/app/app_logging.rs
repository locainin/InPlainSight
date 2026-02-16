use gtk::prelude::*;
use gtk4 as gtk;

use std::collections::HashMap;
use std::ffi::OsString;

use crate::command_builder::CommandExecution;

#[derive(Clone, Copy)]
pub enum LogLevel {
    Info,
    Success,
    Warning,
    Error,
}

impl LogLevel {
    // Uppercase labels keep log scanning consistent across operations
    fn as_text(self) -> &'static str {
        match self {
            Self::Info => "INFO",
            Self::Success => "SUCCESS",
            Self::Warning => "WARNING",
            Self::Error => "ERROR",
        }
    }
}

// Clear log output and reset status label
pub fn wire_clear_log_button(
    clear_logs_button: &gtk::Button,
    log_buffer: &gtk::TextBuffer,
    status_label: &gtk::Label,
) {
    let log_buffer_clone = log_buffer.clone();
    let status_label_clone = status_label.clone();

    clear_logs_button.connect_clicked(move |_| {
        log_buffer_clone.set_text("");
        status_label_clone.set_text("Log cleared");
        status_label_clone.add_css_class("status-ready");
        status_label_clone.remove_css_class("status-ok");
        status_label_clone.remove_css_class("status-fail");
    });
}

// Render command exit information and streams into the shared log panel
pub fn render_command_result(
    operation_name: &str,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
    command_execution: CommandExecution,
) {
    let was_successful = command_execution.exit_code == Some(0);
    let status_text = if was_successful {
        "Command completed successfully"
    } else {
        "Command failed"
    };

    status_label.set_text(status_text);
    status_label.remove_css_class("status-ready");
    status_label.remove_css_class("status-ok");
    status_label.remove_css_class("status-fail");
    if was_successful {
        status_label.add_css_class("status-ok");
    } else {
        status_label.add_css_class("status-fail");
    }

    append_structured_log(
        log_buffer,
        operation_name,
        if was_successful {
            LogLevel::Success
        } else {
            LogLevel::Error
        },
        &format!(
            "command finished with exit code {}",
            format_exit_code(command_execution.exit_code)
        ),
    );

    if was_successful {
        let completion_text = match operation_name {
            "hide" => "payload encrypted and embedded successfully",
            "extract" => "payload extracted and decrypted successfully",
            _ => "operation completed successfully",
        };
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Success,
            completion_text,
        );
    }

    if !command_execution.stdout_text.trim().is_empty() {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Info,
            "captured stdout",
        );
        append_multiline_block(log_buffer, command_execution.stdout_text.trim_end());
    }

    if !command_execution.stderr_text.trim().is_empty() {
        let stderr_level = if was_successful {
            LogLevel::Warning
        } else {
            LogLevel::Error
        };
        append_structured_log(log_buffer, operation_name, stderr_level, "captured stderr");
        append_multiline_block(log_buffer, command_execution.stderr_text.trim_end());
    }

    if !was_successful {
        append_common_failure_hints(operation_name, log_buffer, &command_execution.stderr_text);
    }

    append_log_line(
        log_buffer,
        "------------------------------------------------------------",
    );
}

pub fn append_structured_log(
    log_buffer: &gtk::TextBuffer,
    operation_name: &str,
    level: LogLevel,
    message_text: &str,
) {
    // Each row has timestamp + scope + severity to simplify troubleshooting
    append_log_line(
        log_buffer,
        &format!(
            "[{}] [{}] [{}] {}",
            timestamp_seconds(),
            operation_name,
            level.as_text(),
            message_text
        ),
    );
}

// Append a line at end of log buffer
pub fn append_log_line(log_buffer: &gtk::TextBuffer, line_text: &str) {
    let mut end_iter = log_buffer.end_iter();
    log_buffer.insert(&mut end_iter, line_text);
    log_buffer.insert(&mut end_iter, "\n");
}

fn quote_for_log(argument_text: &str) -> String {
    // Log output is meant to be copy-pastable into a shell for debugging
    // Single-quote escaping keeps behavior deterministic across shells
    if argument_text.is_empty() {
        return "''".to_string();
    }

    let needs_quotes = argument_text.chars().any(|character| {
        character.is_whitespace() || character == '\'' || character == '"' || character == '\\'
    });

    if !needs_quotes {
        return argument_text.to_string();
    }

    let mut quoted = String::new();
    quoted.push('\'');
    for character in argument_text.chars() {
        if character == '\'' {
            quoted.push_str("'\\''");
        } else {
            quoted.push(character);
        }
    }
    quoted.push('\'');
    quoted
}

pub fn render_cli_invocation_for_log(
    cli_binary_path: &str,
    argument_list: &[OsString],
    replacement_pairs: &[(String, String)],
) -> String {
    // Build a stable mapping so callers can redact sensitive argv values
    // Replacement is exact-match only to avoid accidental partial substitutions
    let mut replacements: HashMap<&str, &str> = HashMap::new();
    for (original_value, replacement_value) in replacement_pairs {
        replacements.insert(original_value.as_str(), replacement_value.as_str());
    }

    let mut rendered_parts: Vec<String> = Vec::new();
    rendered_parts.push(quote_for_log(cli_binary_path));
    for argument_value in argument_list {
        let raw_text = argument_value.to_string_lossy();
        let substituted = replacements
            .get(raw_text.as_ref())
            .copied()
            .unwrap_or(raw_text.as_ref());
        rendered_parts.push(quote_for_log(substituted));
    }

    rendered_parts.join(" ")
}

fn append_multiline_block(log_buffer: &gtk::TextBuffer, block_text: &str) {
    // Indent streamed process output to visually separate it from structured rows
    for line_text in block_text.lines() {
        append_log_line(log_buffer, &format!("  {}", line_text));
    }
}

fn format_exit_code(exit_code: Option<i32>) -> String {
    match exit_code {
        Some(code_value) => code_value.to_string(),
        None => "signal".to_string(),
    }
}

fn timestamp_seconds() -> String {
    // Unix seconds avoid locale/timezone differences in issue reports
    let seconds_since_epoch = std::time::SystemTime::now()
        .duration_since(std::time::UNIX_EPOCH)
        .map(|duration_value| duration_value.as_secs())
        .unwrap_or(0);
    seconds_since_epoch.to_string()
}

fn append_common_failure_hints(
    operation_name: &str,
    log_buffer: &gtk::TextBuffer,
    stderr_text: &str,
) {
    let stderr_lowercase = stderr_text.to_ascii_lowercase();

    if stderr_lowercase.contains("unsupported operation") {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Warning,
            "an unsupported mode or format was requested in this build",
        );
    }

    if stderr_lowercase.contains("cover capacity is insufficient") {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Warning,
            "cover image does not have enough capacity for this payload",
        );
    }

    if stderr_lowercase.contains("input exceeds configured limits") {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Warning,
            "input exceeded project bounds; payload/file/image limits are enforced before processing",
        );
    }

    if stderr_lowercase.contains("hide preflight failed: payload exceeds safe limits") {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Warning,
            "preflight showed payload bytes above cover capacity or project cap; see detailed size lines in stderr",
        );
    }

    if stderr_lowercase.contains("authentication failure")
        || stderr_lowercase.contains("extract failed")
    {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Warning,
            "credentials were invalid or the image did not contain a valid payload",
        );
    }

    if stderr_lowercase.contains("failed to launch cli binary")
        || stderr_lowercase.contains("no such file or directory")
    {
        append_structured_log(
            log_buffer,
            operation_name,
            LogLevel::Warning,
            "cli binary path is invalid; open Advanced Settings and set a valid inplainsight path",
        );
    }
}
