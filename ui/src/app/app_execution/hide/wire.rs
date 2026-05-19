// Hide panel signal wiring
//
// This file only gathers inputs at click time
// Heavy work runs in background tasks or focused helper modules

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

#[derive(Clone)]
struct HideExecutionWidgets {
    parent_window: gtk::Window,
    run_button: gtk::Button,
    cover_entry: gtk::Entry,
    payload_file_entry: gtk::Entry,
    output_entry: gtk::Entry,
    output_dir_entry: gtk::Entry,
    output_pattern_entry: gtk::Entry,
    passphrase_file_entry: gtk::Entry,
    passphrase_text_entry: gtk::PasswordEntry,
    passphrase_confirm_entry: gtk::PasswordEntry,
    method_dropdown: gtk::DropDown,
    payload_source_dropdown: gtk::DropDown,
    payload_text_view: gtk::TextView,
    passphrase_source_dropdown: gtk::DropDown,
    cli_path_entry: gtk::Entry,
    status_label: gtk::Label,
    log_buffer: gtk::TextBuffer,
}

pub fn wire_hide_execution(
    hide_panel: &HidePanel,
    parent_window: &gtk::Window,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) {
    let widgets = HideExecutionWidgets {
        parent_window: parent_window.clone(),
        run_button: hide_panel.run_button.clone(),
        cover_entry: hide_panel.cover_field.path_entry.clone(),
        payload_file_entry: hide_panel.payload_file_field.path_entry.clone(),
        output_entry: hide_panel.output_field.path_entry.clone(),
        output_dir_entry: hide_panel.output_dir_field.path_entry.clone(),
        output_pattern_entry: hide_panel.output_pattern_entry.clone(),
        passphrase_file_entry: hide_panel.passphrase_file_field.path_entry.clone(),
        passphrase_text_entry: hide_panel.passphrase_text_entry.clone(),
        passphrase_confirm_entry: hide_panel.passphrase_confirm_entry.clone(),
        method_dropdown: hide_panel.method_dropdown.clone(),
        payload_source_dropdown: hide_panel.payload_source_dropdown.clone(),
        payload_text_view: hide_panel.payload_text_view.clone(),
        passphrase_source_dropdown: hide_panel.passphrase_source_dropdown.clone(),
        cli_path_entry: cli_path_entry.clone(),
        status_label: status_label.clone(),
        log_buffer: log_buffer.clone(),
    };

    hide_panel.run_button.connect_clicked(move |_| {
        start_hide_from_widgets(&widgets);
    });
}

fn start_hide_from_widgets(widgets: &HideExecutionWidgets) {
    let inputs = match collect_hide_execution_inputs(widgets) {
        Ok(value) => value,
        Err(validation_error) => {
            set_validation_error(widgets, &validation_error);
            return;
        }
    };

    set_preflight_running(widgets);
    widgets.run_button.set_sensitive(false);

    let ui_handles = HidePreflightUi {
        parent_window: widgets.parent_window.clone(),
        run_button: widgets.run_button.clone(),
        status_label: widgets.status_label.clone(),
        log_buffer: widgets.log_buffer.clone(),
    };

    start_hide_preflight_then_run(ui_handles, inputs);
}

fn collect_hide_execution_inputs(
    widgets: &HideExecutionWidgets,
) -> Result<HideExecutionInputs, String> {
    let payload_mode = selected_payload_mode(&widgets.payload_source_dropdown);
    let passphrase_mode = selected_passphrase_mode(&widgets.passphrase_source_dropdown);
    let cover_path = widgets.cover_entry.text().to_string();
    let payload_file_path = widgets.payload_file_entry.text().to_string();
    let output_path_requested = widgets.output_entry.text().to_string();
    let passphrase_file_path = widgets.passphrase_file_entry.text().to_string();
    let output_path_normalized = normalize_hide_output_path(&cover_path, &output_path_requested)?;

    log_output_normalization(widgets, &output_path_requested, &output_path_normalized);

    let output_dir = widgets.output_dir_entry.text().to_string();
    let output_template = output_pattern_for_cli(widgets.output_pattern_entry.text().as_str())?;

    let payload_resolution = resolve_hide_payload(
        payload_mode,
        passphrase_mode,
        &payload_file_path,
        &widgets.payload_text_view,
        &cover_path,
        &output_path_normalized,
        &passphrase_file_path,
    )?;
    let mut descriptor_guards = payload_resolution.descriptor_guards;
    let (passphrase_path, passphrase_log_text, passphrase_guard) = resolve_hide_passphrase(
        passphrase_mode,
        &passphrase_file_path,
        widgets.passphrase_text_entry.text().as_str(),
        widgets.passphrase_confirm_entry.text().as_str(),
    )?;

    if let Some(passphrase_guard_file) = passphrase_guard {
        descriptor_guards.push(passphrase_guard_file);
    }

    let hide_command = HideCommand {
        cover_path,
        payload_path: payload_resolution.payload_path.clone(),
        output_path: output_path_normalized,
        passphrase_file_path: passphrase_path,
        embed_method: selected_method(&widgets.method_dropdown),
    };

    Ok(HideExecutionInputs {
        cli_binary_path: widgets.cli_path_entry.text().to_string(),
        hide_command,
        payload_log_text: payload_resolution.payload_log_text,
        passphrase_log_text,
        descriptor_guards,
        payload_bytes: payload_resolution.payload_bytes,
        payload_is_regular_file: payload_resolution.payload_is_regular_file,
        output_dir,
        output_template,
    })
}

fn output_pattern_for_cli(pattern_text: &str) -> Result<String, String> {
    let trimmed_pattern = pattern_text.trim();
    if trimmed_pattern.is_empty() {
        return Err("file name pattern is required".to_string());
    }
    if trimmed_pattern.contains('/') || trimmed_pattern.contains('\\') {
        return Err("file name pattern must not contain folders".to_string());
    }
    if trimmed_pattern.contains("%04u") || trimmed_pattern.contains("%u") {
        return Ok(trimmed_pattern.to_string());
    }
    if trimmed_pattern.contains("{index}") {
        return Ok(trimmed_pattern.replace("{index}", "%04u"));
    }
    if trimmed_pattern.contains("{i}") {
        return Ok(trimmed_pattern.replace("{i}", "%04u"));
    }
    Err("file name pattern must include {index}, {i}, %04u, or %u".to_string())
}

fn log_output_normalization(
    widgets: &HideExecutionWidgets,
    output_path_requested: &str,
    output_path_normalized: &str,
) {
    let output_requested_trimmed = output_path_requested.trim();
    if output_requested_trimmed.is_empty() || output_path_normalized == output_requested_trimmed {
        return;
    }

    widgets.output_entry.set_text(output_path_normalized);
    append_structured_log(
        &widgets.log_buffer,
        "hide",
        LogLevel::Info,
        &format!("output extension inferred, normalized path: {output_path_normalized}"),
    );
}

fn set_validation_error(widgets: &HideExecutionWidgets, validation_error: &str) {
    set_status_fail(
        &widgets.status_label,
        &format!("Validation error: {validation_error}"),
    );
    append_structured_log(
        &widgets.log_buffer,
        "hide",
        LogLevel::Error,
        &format!("validation failed: {validation_error}"),
    );
}

fn set_preflight_running(widgets: &HideExecutionWidgets) {
    crate::app::app_ui_helpers::set_status_pending(
        &widgets.status_label,
        "Running preflight planning...",
    );
    append_structured_log(
        &widgets.log_buffer,
        "hide",
        LogLevel::Info,
        "preflight planning started",
    );
}
