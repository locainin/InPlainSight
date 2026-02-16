use gtk::prelude::*;
use gtk4 as gtk;

use crate::app::app_logging::{LogLevel, append_structured_log};
use crate::app::app_types::ExtractPanel;
use crate::command_builder::{EmbedMethod, ExtractCommand, build_extract_arguments};
use crate::validation::{
    validate_extract_typed_passphrase_inputs, validate_typed_passphrase_inputs,
};

use super::descriptor::create_in_memory_descriptor;
use super::runner::run_command_in_background;

// Build extract command from required fields and run extraction in worker thread
pub(crate) fn wire_extract_execution(
    extract_panel: &ExtractPanel,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) {
    // Clone widget handles once so closure ownership stays simple
    let run_button = extract_panel.run_button.clone();
    let input_entry = extract_panel.input_field.path_entry.clone();
    let output_entry = extract_panel.output_field.path_entry.clone();
    let passphrase_text_entry = extract_panel.passphrase_text_entry.clone();
    let cli_path_entry_clone = cli_path_entry.clone();
    let status_label_clone = status_label.clone();
    let log_buffer_clone = log_buffer.clone();

    extract_panel.run_button.connect_clicked(move |_| {
        // Snapshot current form values at click time
        let input_path = input_entry.text().to_string();
        let output_path = output_entry.text().to_string();
        let passphrase_text = passphrase_text_entry.text().to_string();

        // Path validation runs before passphrase descriptor setup
        if let Err(validation_error) =
            validate_extract_typed_passphrase_inputs(&input_path, &output_path)
        {
            status_label_clone.set_text(&format!("Validation error: {}", validation_error));
            status_label_clone.remove_css_class("status-ready");
            status_label_clone.remove_css_class("status-ok");
            status_label_clone.add_css_class("status-fail");
            append_structured_log(
                &log_buffer_clone,
                "extract",
                LogLevel::Error,
                &format!("validation failed: {}", validation_error),
            );
            return;
        }

        if let Err(validation_error) = validate_typed_passphrase_inputs(&passphrase_text, None) {
            status_label_clone.set_text(&format!("Validation error: {}", validation_error));
            status_label_clone.remove_css_class("status-ready");
            status_label_clone.remove_css_class("status-ok");
            status_label_clone.add_css_class("status-fail");
            append_structured_log(
                &log_buffer_clone,
                "extract",
                LogLevel::Error,
                &format!("validation failed: {}", validation_error),
            );
            return;
        }

        let (passphrase_descriptor_path, passphrase_descriptor_file) =
            match create_in_memory_descriptor("inplainsight_passphrase", passphrase_text.as_bytes()) {
                Ok(resolved_values) => resolved_values,
                Err(error_text) => {
                    status_label_clone.set_text("Validation error: passphrase setup failed");
                    status_label_clone.remove_css_class("status-ready");
                    status_label_clone.remove_css_class("status-ok");
                    status_label_clone.add_css_class("status-fail");
                    append_structured_log(
                        &log_buffer_clone,
                        "extract",
                        LogLevel::Error,
                        &format!("passphrase setup failed: {}", error_text),
                    );
                    return;
                }
            };

        let extract_command = ExtractCommand {
            input_path,
            output_path,
            passphrase_file_path: passphrase_descriptor_path,
            embed_method: EmbedMethod::Lsb,
        };

        let cli_binary_path = cli_path_entry_clone.text().to_string();
        let argument_list = build_extract_arguments(&extract_command);

        // Status update happens before spawning worker thread
        status_label_clone.set_text("Running extract operation...");
        status_label_clone.remove_css_class("status-ready");
        status_label_clone.remove_css_class("status-ok");
        status_label_clone.remove_css_class("status-fail");
        append_structured_log(
            &log_buffer_clone,
            "extract",
            LogLevel::Info,
            &format!(
                "starting command: {} extract --input {} --output {} --passphrase-file <typed-passphrase> --method lsb",
                cli_binary_path, extract_command.input_path, extract_command.output_path
            ),
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
