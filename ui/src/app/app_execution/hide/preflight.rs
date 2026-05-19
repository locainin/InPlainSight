// Hide preflight planning and dispatch
//
// Preflight uses `inplainsight info --json` to select the required output shape

use crate::app::app_execution::planning::{HidePreflightPlanError, parse_hide_preflight_json};
use crate::app::app_logging::{LogLevel, append_structured_log, render_cli_invocation_for_log};
use crate::command_builder::{
    InfoCommand, build_hide_arguments, build_info_arguments, run_cli_command,
};

use gtk4::prelude::*;

use super::super::runner::{run_command_in_background, run_task_in_background_with_callback};
use super::detail::log_stdout_snippet_for_json_parse_failure;
use super::split::{run_split_workflow, should_prompt_for_split};
use super::types::{HideExecutionInputs, HidePreflightUi, set_status_fail};

pub(super) fn start_hide_preflight_then_run(
    ui_handles: HidePreflightUi,
    mut inputs: HideExecutionInputs,
) {
    // Planner supports two modes:
    // - payload_path for regular files on disk
    // - payload_bytes for in-memory payloads where a real file path is not used
    let info_payload_path = if inputs.payload_is_regular_file {
        Some(inputs.hide_command.payload_path.clone())
    } else {
        None
    };
    let info_payload_bytes = if inputs.payload_is_regular_file {
        None
    } else {
        Some(inputs.payload_bytes)
    };

    let info_command = InfoCommand {
        cover_path: inputs.hide_command.cover_path.clone(),
        payload_path: info_payload_path,
        payload_bytes: info_payload_bytes,
        embed_method: inputs.hide_command.embed_method,
    };
    let info_arguments = build_info_arguments(&info_command);

    append_structured_log(
        &ui_handles.log_buffer,
        "hide",
        LogLevel::Info,
        &format!(
            "starting preflight: {}",
            render_cli_invocation_for_log(&inputs.cli_binary_path, &info_arguments, &[])
        ),
    );

    // Keep in-memory descriptors alive until the final hide command is launched
    // This avoids the CLI reading from closed descriptor paths
    let descriptor_guards_for_hide = std::mem::take(&mut inputs.descriptor_guards);
    let cli_binary_path_for_preflight = inputs.cli_binary_path.clone();

    // Run the CLI in a worker thread to keep the UI responsive
    run_task_in_background_with_callback(
        move || run_cli_command(&cli_binary_path_for_preflight, &info_arguments),
        move |preflight_result| {
            handle_hide_preflight_completion(
                ui_handles,
                inputs,
                descriptor_guards_for_hide,
                preflight_result,
            );
        },
    );
}

fn handle_hide_preflight_completion(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    descriptor_guards_for_hide: Vec<std::fs::File>,
    preflight_result: Result<crate::command_builder::CommandExecution, String>,
) {
    // Treat any spawn failure as a preflight failure
    // The error text is already suitable for logs
    let info_execution = match preflight_result {
        Ok(value) => value,
        Err(error_text) => {
            set_status_fail(&ui.status_label, "Preflight failed");
            append_structured_log(&ui.log_buffer, "hide", LogLevel::Error, &error_text);
            ui.run_button.set_sensitive(true);
            return;
        }
    };

    // Preflight must exit cleanly so the JSON plan can be trusted
    // Non-zero exit codes are surfaced as error logs
    if info_execution.exit_code != Some(0) {
        set_status_fail(&ui.status_label, "Preflight failed");
        append_structured_log(
            &ui.log_buffer,
            "hide",
            LogLevel::Error,
            "preflight command returned non-zero exit code",
        );
        if !info_execution.stderr_text.trim().is_empty() {
            append_structured_log(&ui.log_buffer, "hide", LogLevel::Error, "captured stderr");
            for line_text in info_execution.stderr_text.trim_end().lines() {
                append_structured_log(&ui.log_buffer, "hide", LogLevel::Error, line_text);
            }
        }
        ui.run_button.set_sensitive(true);
        return;
    }

    // JSON parsing is strict by design
    // Any schema mismatch should fail closed to avoid incorrect UI planning
    let preflight_plan = match parse_hide_preflight_json(&info_execution) {
        Ok(value) => value,
        Err(error_text) => {
            if matches!(
                error_text,
                HidePreflightPlanError::UnsupportedSchemaVersion(_)
            ) {
                set_status_fail(&ui.status_label, "UI and CLI versions are incompatible");
                append_structured_log(
                    &ui.log_buffer,
                    "hide",
                    LogLevel::Error,
                    "update the UI or the inplainsight CLI so both support the same info --json schema",
                );
            } else {
                set_status_fail(&ui.status_label, "Preflight JSON parse failed");
            }

            append_structured_log(
                &ui.log_buffer,
                "hide",
                LogLevel::Error,
                &error_text.to_string(),
            );
            log_stdout_snippet_for_json_parse_failure(&ui.log_buffer, &info_execution.stdout_text);

            ui.run_button.set_sensitive(true);
            return;
        }
    };

    append_structured_log(
        &ui.log_buffer,
        "hide",
        LogLevel::Info,
        &format!(
            "preflight plan: cover {} {}x{} ({} channels, {} decoded bytes), payload {} bytes, single-image capacity {} bytes, per-image cap {} bytes, required images {}, limit {}, output-cap-risk {}",
            preflight_plan.cover_format,
            preflight_plan.cover_width,
            preflight_plan.cover_height,
            preflight_plan.cover_channels,
            preflight_plan.cover_decoded_bytes,
            preflight_plan.payload_bytes,
            preflight_plan.max_payload_by_cover_bytes,
            preflight_plan.max_payload_per_shard,
            preflight_plan.required_shards,
            preflight_plan.limiting_factor,
            preflight_plan.plan_output_cap_risk,
        ),
    );

    if should_prompt_for_split(&preflight_plan) {
        let output_dir_path = if inputs.output_dir.trim().is_empty() {
            crate::app::app_execution::hide::split::derive_split_output_dir(
                &inputs.hide_command.output_path,
            )
        } else {
            std::path::PathBuf::from(inputs.output_dir.clone())
        };

        append_structured_log(
            &ui.log_buffer,
            "hide",
            LogLevel::Info,
            &format!(
                "preflight requires split output: {} images will be created",
                preflight_plan.required_shards
            ),
        );

        run_split_workflow(
            ui,
            inputs,
            preflight_plan.required_shards,
            output_dir_path,
            descriptor_guards_for_hide,
        );
        return;
    }

    // Single-hide runs when the payload fits in one image
    run_single_hide(ui, inputs, descriptor_guards_for_hide);
}

fn run_single_hide(
    ui: HidePreflightUi,
    inputs: HideExecutionInputs,
    descriptor_guards_for_hide: Vec<std::fs::File>,
) {
    // Single-hide always targets a specific output file path
    let hide_arguments = build_hide_arguments(&inputs.hide_command);
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
            render_cli_invocation_for_log(&inputs.cli_binary_path, &hide_arguments, &replacements)
        ),
    );

    // Status text is kept short so it fits in narrow layouts
    crate::app::app_ui_helpers::set_status_pending(
        &ui.status_label,
        "Encrypting and hiding payload...",
    );

    // The actual CLI run happens in the background runner
    // The guard list is moved so descriptor-backed paths remain valid until completion
    run_command_in_background(
        "hide",
        inputs.cli_binary_path,
        hide_arguments,
        descriptor_guards_for_hide,
        ui.run_button,
        ui.status_label,
        ui.log_buffer,
    );
}
