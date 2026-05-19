// Split-hide workflow
//
// Split mode generates multiple shard images when payload does not fit in one cover image
// This file contains the UI prompts and the split-specific CLI invocation

use gtk::prelude::*;
use gtk4 as gtk;

use crate::app::app_execution::planning::HidePreflightPlan;
use crate::app::app_logging::{LogLevel, append_structured_log, render_cli_invocation_for_log};
use crate::app::app_ui_helpers::show_choice_dialog;
use crate::command_builder::build_hide_split_arguments;

use super::super::runner::run_command_in_background;
use super::types::{HideExecutionInputs, HidePreflightUi, set_status_fail, set_status_ready};

pub(super) const fn should_prompt_for_split(preflight_plan: &HidePreflightPlan) -> bool {
    // Only split when a payload was provided and a single cover cannot fit it
    // This avoids prompting during cover-only planning checks
    preflight_plan.payload_provided && !preflight_plan.fits_single
}

pub(super) fn derive_split_output_dir(output_path: &str) -> std::path::PathBuf {
    // Split mode writes shard images to a folder
    // The folder name is deterministic so repeated runs are easy to locate
    let requested_output_path = std::path::PathBuf::from(output_path);
    let output_parent = requested_output_path.parent().map_or_else(
        || std::path::PathBuf::from("."),
        std::path::Path::to_path_buf,
    );
    let output_stem = requested_output_path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("hidden_payload");
    output_parent.join(format!("{output_stem}_shards"))
}

fn expected_split_shard_name(shard_index: u64, template_text: &str) -> Result<String, String> {
    // shard_index is bounded by planner caps, but still validate defensively
    if shard_index > 9999u64 {
        return Err("shard index exceeds supported naming width".to_string());
    }

    let Some(percent_index) = template_text.find('%') else {
        return Err("split output template is missing an index slot".to_string());
    };
    let Some(u_offset) = template_text[percent_index..].find('u') else {
        return Err("split output template is missing an unsigned index marker".to_string());
    };
    let u_index = percent_index + u_offset;
    let format_text = &template_text[percent_index + 1..u_index];
    let pad_width = format_text
        .strip_prefix('0')
        .and_then(|width_text| width_text.parse::<usize>().ok())
        .unwrap_or(0);
    let rendered_index = if pad_width > 0 {
        format!("{shard_index:0pad_width$}")
    } else {
        shard_index.to_string()
    };

    Ok(format!(
        "{}{}{}",
        &template_text[..percent_index],
        rendered_index,
        &template_text[u_index + 1..]
    ))
}

fn find_existing_expected_shards(
    output_dir: &std::path::Path,
    shard_count: u64,
    template_text: &str,
) -> Result<Vec<std::path::PathBuf>, String> {
    // Preflight JSON comes from an external binary, so treat it as untrusted
    if shard_count == 0 {
        return Ok(Vec::new());
    }
    if shard_count > 65535 {
        return Err("planned shard count is unexpectedly large".to_string());
    }

    let mut existing_paths: Vec<std::path::PathBuf> = Vec::new();

    // Only check the expected shard filenames
    // This avoids touching unrelated files that may be present in the folder
    for shard_index in 0..shard_count {
        let shard_name = expected_split_shard_name(shard_index, template_text)?;
        let shard_path = output_dir.join(shard_name);
        if shard_path.exists() {
            existing_paths.push(shard_path);
        }
    }

    Ok(existing_paths)
}

pub(super) fn run_split_workflow(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    required_shards: u64,
    output_dir: std::path::PathBuf,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // The C CLI expects the output directory to exist
    // Creating it in the UI keeps error messages more direct
    if let Err(error_value) = std::fs::create_dir_all(&output_dir) {
        set_status_fail(&ui.status_label, "Split output directory creation failed");
        append_structured_log(
            &ui.log_buffer,
            "hide",
            LogLevel::Error,
            &format!("failed creating split output directory: {error_value}"),
        );
        ui.run_button.set_sensitive(true);
        return;
    }

    // Detect a common footgun:
    // split mode is re-run into a folder with existing shard outputs
    let existing_shards = match find_existing_expected_shards(
        &output_dir,
        required_shards,
        &inputs.output_template,
    ) {
        Ok(values) => values,
        Err(error_text) => {
            set_status_fail(&ui.status_label, "Split output preflight failed");
            append_structured_log(&ui.log_buffer, "hide", LogLevel::Error, &error_text);
            ui.run_button.set_sensitive(true);
            return;
        }
    };

    if existing_shards.is_empty() {
        // Fast path when nothing would be overwritten
        run_split_hide_with_output_dir(ui, inputs, &output_dir, descriptor_guards_for_hide);
        return;
    }

    // Prompt for how to handle existing shard files
    // This keeps the default policy as "do not overwrite" unless explicitly confirmed
    prompt_for_existing_shards_action(
        ui,
        inputs,
        output_dir,
        existing_shards,
        descriptor_guards_for_hide,
    );
}

fn prompt_for_existing_shards_action(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    output_dir: std::path::PathBuf,
    existing_shards: Vec<std::path::PathBuf>,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    let parent_window_for_dialog = ui.parent_window.clone();
    let detail_text = format!(
        "Found {} existing shard files that would be overwritten\n\nChoose a different folder, clear existing shards, or cancel",
        existing_shards.len()
    );
    show_choice_dialog(
        &parent_window_for_dialog,
        "Output folder already contains shard outputs",
        &detail_text,
        &[
            "Cancel",
            "Choose different folder",
            "Clear existing shards and continue",
        ],
        2,
        move |overwrite_index| {
            if overwrite_index == 0 {
                // Cancellation restores the UI to a ready state
                set_status_ready(&ui.status_label, "Hide cancelled");
                append_structured_log(
                    &ui.log_buffer,
                    "hide",
                    LogLevel::Info,
                    "user cancelled split hide after existing shard warning",
                );
                ui.run_button.set_sensitive(true);
                return;
            }

            if overwrite_index == 1 {
                // Folder picker avoids touching unrelated files in the current folder
                choose_different_output_folder_then_run(ui, inputs, descriptor_guards_for_hide);
                return;
            }

            // Clear only the expected shard files and keep unrelated files untouched
            for shard_path in existing_shards {
                let _ = std::fs::remove_file(&shard_path);
            }

            // Clearing expected shards is treated as an explicit overwrite approval
            run_split_hide_with_output_dir(ui, inputs, &output_dir, descriptor_guards_for_hide);
        },
    );
}

fn choose_different_output_folder_then_run(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // Folder selection is done via GTK so portals and desktop integrations work
    let folder_dialog = gtk::FileDialog::builder()
        .title("Choose output folder for shard images")
        .build();

    let parent_window_for_dialog = ui.parent_window.clone();
    folder_dialog.select_folder(
        Some(&parent_window_for_dialog),
        None::<&gtk::gio::Cancellable>,
        move |folder_result| {
            let folder_file = match folder_result {
                Ok(value) => value,
                Err(error_value) => {
                    if error_value.matches(gtk::gio::IOErrorEnum::Cancelled) {
                        // Cancel is not an error and should not spam the log
                        set_status_ready(&ui.status_label, "Hide cancelled");
                        ui.run_button.set_sensitive(true);
                        return;
                    }

                    set_status_fail(&ui.status_label, "Folder selection failed");
                    append_structured_log(
                        &ui.log_buffer,
                        "hide",
                        LogLevel::Error,
                        &format!("failed to choose output folder: {error_value}"),
                    );
                    ui.run_button.set_sensitive(true);
                    return;
                }
            };

            // Only local filesystem paths are supported by the CLI
            let Some(folder_path) = folder_file.path() else {
                set_status_fail(&ui.status_label, "Only local output folders are supported");
                append_structured_log(
                    &ui.log_buffer,
                    "hide",
                    LogLevel::Error,
                    "selected output folder did not map to a local filesystem path",
                );
                ui.run_button.set_sensitive(true);
                return;
            };

            // Directory is created so the split hide can start immediately
            if let Err(error_value) = std::fs::create_dir_all(&folder_path) {
                set_status_fail(&ui.status_label, "Split output directory creation failed");
                append_structured_log(
                    &ui.log_buffer,
                    "hide",
                    LogLevel::Error,
                    &format!("failed creating split output directory: {error_value}"),
                );
                ui.run_button.set_sensitive(true);
                return;
            }

            run_split_hide_with_output_dir(ui, inputs, &folder_path, descriptor_guards_for_hide);
        },
    );
}

fn run_split_hide_with_output_dir(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    output_dir: &std::path::Path,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // Split mode uses --output-dir and a fixed naming template inside the CLI
    // Replacements keep logs readable and avoid leaking descriptor paths
    let split_arguments = build_hide_split_arguments(
        &inputs.hide_command.cover_path,
        &inputs.hide_command.payload_path,
        output_dir.to_string_lossy().as_ref(),
        &inputs.output_template,
        &inputs.hide_command.passphrase_file_path,
        inputs.hide_command.embed_method,
    );

    let replacements = vec![
        (
            inputs.hide_command.payload_path.clone(),
            inputs.payload_log_text,
        ),
        (
            inputs.hide_command.passphrase_file_path.clone(),
            inputs.passphrase_log_text,
        ),
    ];

    append_structured_log(
        &ui.log_buffer,
        "hide",
        LogLevel::Info,
        &format!(
            "planned command: {}",
            render_cli_invocation_for_log(&inputs.cli_binary_path, &split_arguments, &replacements)
        ),
    );

    // Status text is short so it fits in narrow layouts
    crate::app::app_ui_helpers::set_status_pending(
        &ui.status_label,
        "Encrypting and splitting payload...",
    );

    // Background runner owns the CLI process and streams logs back to the UI
    run_command_in_background(
        "hide",
        inputs.cli_binary_path,
        split_arguments,
        descriptor_guards_for_hide,
        ui.run_button,
        ui.status_label,
        ui.log_buffer,
    );
}
