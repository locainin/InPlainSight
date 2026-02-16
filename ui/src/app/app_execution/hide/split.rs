// Split-hide workflow
//
// Split mode generates multiple shard images when payload does not fit in one cover image
// This file contains the UI prompts and the split-specific CLI invocation

use gtk::prelude::*;
use gtk4 as gtk;

use crate::app::app_execution::planning::HidePreflightPlan;
use crate::app::app_logging::{LogLevel, append_structured_log, render_cli_invocation_for_log};
use crate::command_builder::build_hide_split_arguments;

use super::super::runner::run_command_in_background;
use super::detail::build_split_warning_detail;
use super::types::{HideExecutionInputs, HidePreflightUi, set_status_fail, set_status_ready};

pub(super) fn should_prompt_for_split(preflight_plan: &HidePreflightPlan) -> bool {
    // Only split when a payload was provided and a single cover cannot fit it
    // This avoids prompting during cover-only planning checks
    preflight_plan.payload_provided && !preflight_plan.fits_single
}

pub(super) fn derive_split_output_dir(output_path: &str) -> std::path::PathBuf {
    // Split mode writes shard images to a folder
    // The folder name is deterministic so repeated runs are easy to locate
    let requested_output_path = std::path::PathBuf::from(output_path);
    let output_parent = requested_output_path
        .parent()
        .map(|value| value.to_path_buf())
        .unwrap_or_else(|| std::path::PathBuf::from("."));
    let output_stem = requested_output_path
        .file_stem()
        .and_then(|value| value.to_str())
        .unwrap_or("hidden_payload");
    output_parent.join(format!("{}_shards", output_stem))
}

fn infer_split_shard_extension_from_cover_path(cover_path: &str) -> &'static str {
    // Split output stays lossless so embedded bits survive encoding
    // Lossy cover inputs still produce lossless shard outputs
    let extension = std::path::Path::new(cover_path)
        .extension()
        .and_then(|value| value.to_str())
        .map(|value| value.to_ascii_lowercase())
        .unwrap_or_default();

    match extension.as_str() {
        "jxl" => "jxl",
        "bmp" | "dib" => "bmp",
        "ppm" => "ppm",
        _ => "png",
    }
}

fn expected_split_shard_name(shard_index: u64, extension: &str) -> Result<String, String> {
    // shard_index is bounded by planner caps, but still validate defensively
    if shard_index > 9999u64 {
        return Err("shard index exceeds supported naming width".to_string());
    }

    // Four digits keeps filenames aligned and easy to scan
    Ok(format!("shard_{:04}.{}", shard_index, extension))
}

fn find_existing_expected_shards(
    output_dir: &std::path::Path,
    shard_count: u64,
    extension: &str,
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
        let shard_name = expected_split_shard_name(shard_index, extension)?;
        let shard_path = output_dir.join(shard_name);
        if shard_path.exists() {
            existing_paths.push(shard_path);
        }
    }

    Ok(existing_paths)
}

pub(super) fn prompt_split_then_run(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    preflight_plan: HidePreflightPlan,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // Output directory is derived from the requested output filename
    // This keeps shard artifacts near the intended output location
    let output_dir = derive_split_output_dir(&inputs.hide_command.output_path);
    let detail_text = build_split_warning_detail(&preflight_plan, &output_dir);

    // Split confirmation avoids surprising the filesystem with many new files
    let dialog = gtk::AlertDialog::builder()
        .message("Payload does not fit in a single image")
        .detail(detail_text)
        .build();
    dialog.set_buttons(&["Cancel", "Split into multiple images"]);
    dialog.set_default_button(1);
    dialog.set_cancel_button(0);

    let parent_window_for_dialog = ui.parent_window.clone();
    dialog.choose(
        Some(&parent_window_for_dialog),
        None::<&gtk::gio::Cancellable>,
        move |choice| {
            let response_index = choice.unwrap_or(0);
            if response_index != 1 {
                // Cancellation restores a ready status and re-enables the run button
                set_status_ready(&ui.status_label, "Hide cancelled");
                append_structured_log(
                    &ui.log_buffer,
                    "hide",
                    LogLevel::Info,
                    "user cancelled split hide after preflight warning",
                );
                ui.run_button.set_sensitive(true);
                return;
            }

            // Confirmation continues into the split workflow
            run_split_after_confirm(
                ui,
                inputs,
                preflight_plan,
                output_dir,
                descriptor_guards_for_hide,
            );
        },
    );
}

fn run_split_after_confirm(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    preflight_plan: HidePreflightPlan,
    output_dir: std::path::PathBuf,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // Extension is used for expected shard names during the "existing outputs" check
    // Split output format is based on cover format but stays lossless
    let cover_extension_for_shards =
        infer_split_shard_extension_from_cover_path(&inputs.hide_command.cover_path);

    // The C CLI expects the output directory to exist
    // Creating it in the UI keeps error messages more direct
    if let Err(error_value) = std::fs::create_dir_all(&output_dir) {
        set_status_fail(&ui.status_label, "Split output directory creation failed");
        append_structured_log(
            &ui.log_buffer,
            "hide",
            LogLevel::Error,
            &format!("failed creating split output directory: {}", error_value),
        );
        ui.run_button.set_sensitive(true);
        return;
    }

    // Detect a common footgun:
    // split mode is re-run into a folder with existing shard outputs
    let existing_shards = match find_existing_expected_shards(
        &output_dir,
        preflight_plan.required_shards,
        cover_extension_for_shards,
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
        run_split_hide_with_output_dir(ui, inputs, output_dir, descriptor_guards_for_hide);
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
    // Existing shard files are risky because they can be mistaken as fresh output
    // This dialog forces an explicit choice to prevent accidental overwrites
    let overwrite_dialog = gtk::AlertDialog::builder()
        .message("Output folder already contains shard outputs")
        .detail(format!(
            "Found {} existing shard files that would be overwritten\n\nChoose a different folder, clear existing shards, or cancel",
            existing_shards.len()
        ))
        .build();
    overwrite_dialog.set_buttons(&[
        "Cancel",
        "Choose different folder",
        "Clear existing shards and continue",
    ]);
    overwrite_dialog.set_default_button(2);
    overwrite_dialog.set_cancel_button(0);

    let parent_window_for_dialog = ui.parent_window.clone();
    overwrite_dialog.choose(
        Some(&parent_window_for_dialog),
        None::<&gtk::gio::Cancellable>,
        move |overwrite_choice| {
            let overwrite_index = overwrite_choice.unwrap_or(0);
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
            run_split_hide_with_output_dir(ui, inputs, output_dir, descriptor_guards_for_hide);
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
                        &format!("failed to choose output folder: {}", error_value),
                    );
                    ui.run_button.set_sensitive(true);
                    return;
                }
            };

            // Only local filesystem paths are supported by the CLI
            let folder_path = match folder_file.path() {
                Some(value) => value,
                None => {
                    set_status_fail(&ui.status_label, "Only local output folders are supported");
                    append_structured_log(
                        &ui.log_buffer,
                        "hide",
                        LogLevel::Error,
                        "selected output folder did not map to a local filesystem path",
                    );
                    ui.run_button.set_sensitive(true);
                    return;
                }
            };

            // Directory is created so the split hide can start immediately
            if let Err(error_value) = std::fs::create_dir_all(&folder_path) {
                set_status_fail(&ui.status_label, "Split output directory creation failed");
                append_structured_log(
                    &ui.log_buffer,
                    "hide",
                    LogLevel::Error,
                    &format!("failed creating split output directory: {}", error_value),
                );
                ui.run_button.set_sensitive(true);
                return;
            }

            run_split_hide_with_output_dir(ui, inputs, folder_path, descriptor_guards_for_hide);
        },
    );
}

fn run_split_hide_with_output_dir(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    output_dir: std::path::PathBuf,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // Split mode uses --output-dir and a fixed naming template inside the CLI
    // Replacements keep logs readable and avoid leaking descriptor paths
    let split_arguments = build_hide_split_arguments(
        &inputs.hide_command.cover_path,
        &inputs.hide_command.payload_path,
        output_dir.to_string_lossy().as_ref(),
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
    ui.status_label
        .set_text("Encrypting and splitting payload...");
    ui.status_label.remove_css_class("status-ready");
    ui.status_label.remove_css_class("status-ok");
    ui.status_label.remove_css_class("status-fail");

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
