use gtk::prelude::*;
use gtk4 as gtk;
use std::ffi::OsString;

use crate::app::app_fields::selected_extract_input_mode;
use crate::app::app_logging::{LogLevel, append_structured_log};
use crate::app::app_types::{ExtractInputMode, ExtractPanel};
use crate::app::app_ui_helpers::{set_status_fail, set_status_pending};
use crate::command_builder::{EmbedMethod, ExtractCommand, build_extract_arguments};
use crate::validation::{
    validate_extract_folder_typed_passphrase_inputs, validate_extract_typed_passphrase_inputs,
    validate_typed_passphrase_inputs,
};

use super::descriptor::create_in_memory_descriptor;
use super::runner::run_command_in_background;

struct ExtractFormSnapshot {
    input_path: String,
    input_dir: String,
    input_mode: ExtractInputMode,
    output_path: String,
    passphrase_text: String,
}

// Build extract command from required fields and run extraction in worker thread
pub fn wire_extract_execution(
    extract_panel: &ExtractPanel,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) {
    // Clone widget handles once so closure ownership stays simple
    let run_button = extract_panel.run_button.clone();
    let input_entry = extract_panel.input_field.path_entry.clone();
    let input_dir_entry = extract_panel.input_dir_field.path_entry.clone();
    let input_source_dropdown = extract_panel.input_source_dropdown.clone();
    let output_entry = extract_panel.output_field.path_entry.clone();
    let passphrase_text_entry = extract_panel.passphrase_text_entry.clone();
    let cli_path_entry_clone = cli_path_entry.clone();
    let status_label_clone = status_label.clone();
    let log_buffer_clone = log_buffer.clone();

    extract_panel.run_button.connect_clicked(move |_| {
        let form_snapshot = snapshot_extract_form(
            &input_entry,
            &input_dir_entry,
            &input_source_dropdown,
            &output_entry,
            &passphrase_text_entry,
        );

        if let Err(validation_error) = validate_extract_form(&form_snapshot) {
            report_extract_validation_error(
                &status_label_clone,
                &log_buffer_clone,
                &validation_error,
            );
            return;
        }

        let Some((passphrase_descriptor_path, passphrase_descriptor_file)) =
            create_extract_passphrase_descriptor(
                &form_snapshot.passphrase_text,
                &status_label_clone,
                &log_buffer_clone,
            )
        else {
            return;
        };

        let extract_command = build_extract_command(form_snapshot, passphrase_descriptor_path);
        let cli_binary_path = cli_path_entry_clone.text().to_string();
        let argument_list = build_extract_arguments(&extract_command);
        let passphrase_file_path_for_log = extract_command.passphrase_file_path;

        // Status update happens before spawning worker thread
        set_status_pending(&status_label_clone, "Running extract operation...");
        log_extract_command(
            &log_buffer_clone,
            &cli_binary_path,
            &argument_list,
            passphrase_file_path_for_log,
        );

        // Disable button to prevent duplicate concurrent launches
        run_button.set_sensitive(false);

        run_command_in_background(
            "extract",
            cli_binary_path,
            argument_list,
            vec![passphrase_descriptor_file],
            run_button.clone(),
            status_label_clone.clone(),
            log_buffer_clone.clone(),
        );
    });
}

fn snapshot_extract_form(
    input_entry: &gtk::Entry,
    input_dir_entry: &gtk::Entry,
    input_source_dropdown: &gtk::DropDown,
    output_entry: &gtk::Entry,
    passphrase_text_entry: &gtk::PasswordEntry,
) -> ExtractFormSnapshot {
    ExtractFormSnapshot {
        input_path: input_entry.text().to_string(),
        input_dir: input_dir_entry.text().to_string(),
        input_mode: selected_extract_input_mode(input_source_dropdown),
        output_path: output_entry.text().to_string(),
        passphrase_text: passphrase_text_entry.text().to_string(),
    }
}

fn validate_extract_form(snapshot: &ExtractFormSnapshot) -> Result<(), String> {
    // Path validation runs before passphrase descriptor setup
    match snapshot.input_mode {
        ExtractInputMode::File => {
            validate_extract_typed_passphrase_inputs(&snapshot.input_path, &snapshot.output_path)?;
        }
        ExtractInputMode::Folder => validate_extract_folder_typed_passphrase_inputs(
            &snapshot.input_dir,
            &snapshot.output_path,
        )?,
    }

    validate_typed_passphrase_inputs(&snapshot.passphrase_text, None)
}

fn report_extract_validation_error(
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
    validation_error: &str,
) {
    set_status_fail(
        status_label,
        &format!("Validation error: {validation_error}"),
    );
    append_structured_log(
        log_buffer,
        "extract",
        LogLevel::Error,
        &format!("validation failed: {validation_error}"),
    );
}

fn create_extract_passphrase_descriptor(
    passphrase_text: &str,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) -> Option<(String, std::fs::File)> {
    match create_in_memory_descriptor("inplainsight_passphrase", passphrase_text.as_bytes()) {
        Ok(resolved_values) => Some(resolved_values),
        Err(error_text) => {
            set_status_fail(status_label, "Validation error: passphrase setup failed");
            append_structured_log(
                log_buffer,
                "extract",
                LogLevel::Error,
                &format!("passphrase setup failed: {error_text}"),
            );
            None
        }
    }
}

fn build_extract_command(
    snapshot: ExtractFormSnapshot,
    passphrase_descriptor_path: String,
) -> ExtractCommand {
    ExtractCommand {
        input_path: snapshot.input_path,
        input_dir: match snapshot.input_mode {
            ExtractInputMode::Folder => Some(snapshot.input_dir),
            ExtractInputMode::File => None,
        },
        output_path: snapshot.output_path,
        passphrase_file_path: passphrase_descriptor_path,
        embed_method: EmbedMethod::Lsb,
    }
}

fn log_extract_command(
    log_buffer: &gtk::TextBuffer,
    cli_binary_path: &str,
    argument_list: &[OsString],
    passphrase_file_path: String,
) {
    append_structured_log(
        log_buffer,
        "extract",
        LogLevel::Info,
        &format!(
            "starting command: {}",
            crate::app::app_logging::render_cli_invocation_for_log(
                cli_binary_path,
                argument_list,
                &[(passphrase_file_path, "<typed-passphrase>".to_string())],
            )
        ),
    );
}
