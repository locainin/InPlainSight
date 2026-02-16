// Hide panel signal wiring
//
// This file only wires UI events and gathers inputs at click time
// All heavy work runs in background tasks or other helper modules

use gtk::prelude::*;
use gtk4 as gtk;

use crate::app::app_fields::{selected_method, selected_passphrase_mode, selected_payload_mode};
use crate::app::app_logging::{LogLevel, append_structured_log};
use crate::app::app_types::HidePanel;
use crate::command_builder::HideCommand;

use super::super::helpers::normalize_hide_output_path;
use super::preflight::start_hide_preflight_then_run;
use super::resolve::{resolve_hide_passphrase, resolve_hide_payload};
use super::types::{HideExecutionInputs, HidePreflightUi, set_status_fail};

pub(crate) fn wire_hide_execution(
    hide_panel: &HidePanel,
    parent_window: &gtk::Window,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) {
    // Widgets are cloned once so the click handler owns stable references
    // GTK clones are cheap and avoid borrowing issues in long callbacks
    let run_button = hide_panel.run_button.clone();
    let cover_entry = hide_panel.cover_field.path_entry.clone();
    let payload_file_entry = hide_panel.payload_file_field.path_entry.clone();
    let output_entry = hide_panel.output_field.path_entry.clone();
    let passphrase_file_entry = hide_panel.passphrase_file_field.path_entry.clone();
    let passphrase_text_entry = hide_panel.passphrase_text_entry.clone();
    let passphrase_confirm_entry = hide_panel.passphrase_confirm_entry.clone();
    let method_dropdown = hide_panel.method_dropdown.clone();
    let payload_source_dropdown = hide_panel.payload_source_dropdown.clone();
    let payload_text_view = hide_panel.payload_text_view.clone();
    let passphrase_source_dropdown = hide_panel.passphrase_source_dropdown.clone();

    let cli_path_entry_clone = cli_path_entry.clone();
    let status_label_clone = status_label.clone();
    let log_buffer_clone = log_buffer.clone();
    let parent_window_clone = parent_window.clone();

    hide_panel.run_button.connect_clicked(move |_| {
        // Field values are captured at click time and moved into background tasks
        // This avoids racey behavior where the UI changes while the command is running
        let payload_mode = selected_payload_mode(&payload_source_dropdown);
        let passphrase_mode = selected_passphrase_mode(&passphrase_source_dropdown);
        let cover_path = cover_entry.text().to_string();
        let payload_file_path = payload_file_entry.text().to_string();
        let output_path_requested = output_entry.text().to_string();
        let passphrase_file_path = passphrase_file_entry.text().to_string();

        // Output normalization keeps behavior aligned with the C CLI
        // The UI also updates the field so the user sees the exact output path
        let output_path_normalized =
            match normalize_hide_output_path(&cover_path, &output_path_requested) {
                Ok(path_value) => path_value,
                Err(validation_error) => {
                    set_status_fail(
                        &status_label_clone,
                        &format!("Validation error: {}", validation_error),
                    );
                    append_structured_log(
                        &log_buffer_clone,
                        "hide",
                        LogLevel::Error,
                        &format!("validation failed: {}", validation_error),
                    );
                    return;
                }
            };

        let output_requested_trimmed = output_path_requested.trim();
        if !output_requested_trimmed.is_empty()
            && output_path_normalized != output_requested_trimmed
        {
            // Avoid logging on empty or whitespace-only output inputs
            // This prevents confusing "normalized path" messages for blank entries
            output_entry.set_text(&output_path_normalized);
            append_structured_log(
                &log_buffer_clone,
                "hide",
                LogLevel::Info,
                &format!(
                    "output extension inferred, normalized path: {}",
                    output_path_normalized
                ),
            );
        }

        // Payload resolution happens before passphrase resolution
        // This keeps the first error message focused on the selected payload mode
        let payload_resolution = match resolve_hide_payload(
            payload_mode,
            passphrase_mode,
            &payload_file_path,
            &payload_text_view,
            &cover_path,
            &output_path_normalized,
            &passphrase_file_path,
        ) {
            Ok(resolved_values) => resolved_values,
            Err(validation_error) => {
                set_status_fail(
                    &status_label_clone,
                    &format!("Validation error: {}", validation_error),
                );
                append_structured_log(
                    &log_buffer_clone,
                    "hide",
                    LogLevel::Error,
                    &format!("validation failed: {}", validation_error),
                );
                return;
            }
        };

        // Descriptor guards keep memfd-backed payloads alive during execution
        // Without the open file handles, descriptor paths may become invalid
        let mut descriptor_guards = payload_resolution.descriptor_guards;

        // Passphrase resolution can produce a memfd-backed path for typed secrets
        // The CLI reads the passphrase through the provided file path
        let passphrase_resolution = resolve_hide_passphrase(
            passphrase_mode,
            &passphrase_file_path,
            passphrase_text_entry.text().as_str(),
            passphrase_confirm_entry.text().as_str(),
        );

        let (passphrase_path, passphrase_log_text, passphrase_guard) = match passphrase_resolution {
            Ok(resolved_values) => resolved_values,
            Err(validation_error) => {
                set_status_fail(
                    &status_label_clone,
                    &format!("Validation error: {}", validation_error),
                );
                append_structured_log(
                    &log_buffer_clone,
                    "hide",
                    LogLevel::Error,
                    &format!("validation failed: {}", validation_error),
                );
                return;
            }
        };

        if let Some(passphrase_guard_file) = passphrase_guard {
            // Descriptor guard is appended so the file stays open until the worker completes
            descriptor_guards.push(passphrase_guard_file);
        }

        // A HideCommand matches the CLI argument model
        // This is used later to build both preflight and hide invocations
        let hide_command = HideCommand {
            cover_path,
            payload_path: payload_resolution.payload_path.clone(),
            output_path: output_path_normalized,
            passphrase_file_path: passphrase_path,
            embed_method: selected_method(&method_dropdown),
        };

        let cli_binary_path = cli_path_entry_clone.text().to_string();

        // Preflight runs first so the UI can warn about split output
        // This avoids surprising users with many output images
        status_label_clone.set_text("Running preflight planning...");
        status_label_clone.remove_css_class("status-ready");
        status_label_clone.remove_css_class("status-ok");
        status_label_clone.remove_css_class("status-fail");
        append_structured_log(
            &log_buffer_clone,
            "hide",
            LogLevel::Info,
            "preflight planning started",
        );

        // Disable the run button while background work is active
        // This avoids overlapping hide operations and mixed logs
        run_button.set_sensitive(false);

        // UI handles are grouped so later callbacks do not need many parameters
        let ui_handles = HidePreflightUi {
            parent_window: parent_window_clone.clone(),
            run_button: run_button.clone(),
            status_label: status_label_clone.clone(),
            log_buffer: log_buffer_clone.clone(),
        };

        let inputs = HideExecutionInputs {
            cli_binary_path,
            hide_command,
            payload_log_text: payload_resolution.payload_log_text,
            passphrase_log_text,
            descriptor_guards,
            payload_bytes: payload_resolution.payload_bytes,
            payload_is_regular_file: payload_resolution.payload_is_regular_file,
        };

        // Preflight is executed in the background
        // UI updates happen in the completion callback on the GTK thread
        start_hide_preflight_then_run(ui_handles, inputs);
    });
}
