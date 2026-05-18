use gtk::prelude::*;
use gtk4 as gtk;

use super::super::formatting::format_file_size;
use super::naming::preview_name_from_pattern;
use super::types::OutputPlanView;
use crate::app::app_execution::helpers::extract_text_payload;
use crate::app::app_execution::planning::{HidePreflightPlan, parse_hide_preflight_json};
use crate::app::app_execution::runner::run_task_in_background_with_callback;
use crate::app::app_fields::{selected_method, selected_payload_mode};
use crate::app::app_logging::{LogLevel, append_structured_log, render_cli_invocation_for_log};
use crate::app::app_types::{HidePanel, HidePayloadMode};
use crate::command_builder::{
    CommandExecution, InfoCommand, build_info_arguments, run_cli_command,
};

pub fn wire_review_plan_button(
    hide_panel: &HidePanel,
    output_plan_view: &OutputPlanView,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
    workflow_stack: &gtk::Stack,
    review_button: &gtk::Button,
) {
    let cover_entry = hide_panel.cover_field.path_entry.clone();
    let payload_file_entry = hide_panel.payload_file_field.path_entry.clone();
    let payload_source_dropdown = hide_panel.payload_source_dropdown.clone();
    let payload_text_view = hide_panel.payload_text_view.clone();
    let method_dropdown = hide_panel.method_dropdown.clone();
    let output_pattern_entry = hide_panel.output_pattern_entry.clone();
    let cli_path_entry = cli_path_entry.clone();
    let status_label = status_label.clone();
    let log_buffer = log_buffer.clone();
    let workflow_stack = workflow_stack.clone();
    let output_plan_view = output_plan_view.clone();
    let review_button = review_button.clone();

    review_button.clone().connect_clicked(move |_| {
        let plan_inputs = match collect_output_plan_inputs(
            &cover_entry,
            &payload_file_entry,
            &payload_source_dropdown,
            &payload_text_view,
            &method_dropdown,
        ) {
            Ok(value) => value,
            Err(error_text) => {
                set_status_error(&status_label, &error_text);
                append_structured_log(&log_buffer, "hide", LogLevel::Error, &error_text);
                return;
            }
        };

        let cli_binary_path = cli_path_entry.text().to_string();
        let info_arguments = build_info_arguments(&plan_inputs);
        append_structured_log(
            &log_buffer,
            "hide",
            LogLevel::Info,
            &format!(
                "starting output plan: {}",
                render_cli_invocation_for_log(&cli_binary_path, &info_arguments, &[])
            ),
        );
        set_status_pending(&status_label, "Running preflight planning...");
        review_button.set_sensitive(false);
        review_button.set_label("Running Preflight");

        let output_plan_view_for_callback = output_plan_view.clone();
        let workflow_stack_for_callback = workflow_stack.clone();
        let status_label_for_callback = status_label.clone();
        let log_buffer_for_callback = log_buffer.clone();
        let review_button_for_callback = review_button.clone();
        let pattern_entry_for_callback = output_pattern_entry.clone();

        run_task_in_background_with_callback(
            move || run_cli_command(&cli_binary_path, &info_arguments),
            move |preflight_result| {
                handle_output_plan_preflight_result(
                    preflight_result,
                    &output_plan_view_for_callback,
                    &workflow_stack_for_callback,
                    &status_label_for_callback,
                    &log_buffer_for_callback,
                    &review_button_for_callback,
                    pattern_entry_for_callback.text().as_str(),
                );
            },
        );
    });
}

fn collect_output_plan_inputs(
    cover_entry: &gtk::Entry,
    payload_file_entry: &gtk::Entry,
    payload_source_dropdown: &gtk::DropDown,
    payload_text_view: &gtk::TextView,
    method_dropdown: &gtk::DropDown,
) -> Result<InfoCommand, String> {
    let cover_path = cover_entry.text().trim().to_string();
    if cover_path.is_empty() {
        return Err("select a cover image before reviewing the output plan".to_string());
    }

    let (payload_path, payload_bytes) = match selected_payload_mode(payload_source_dropdown) {
        HidePayloadMode::File => {
            let payload_path = payload_file_entry.text().trim().to_string();
            if payload_path.is_empty() {
                return Err("select a payload before reviewing the output plan".to_string());
            }
            (Some(payload_path), None)
        }
        HidePayloadMode::Text => {
            let text_payload = extract_text_payload(payload_text_view)?;
            let payload_length = u64::try_from(text_payload.len())
                .map_err(|_| "text payload is too large to plan".to_string())?;
            (None, Some(payload_length))
        }
    };

    Ok(InfoCommand {
        cover_path,
        payload_path,
        payload_bytes,
        embed_method: selected_method(method_dropdown),
    })
}

fn handle_output_plan_preflight_result(
    preflight_result: Result<CommandExecution, String>,
    output_plan_view: &OutputPlanView,
    workflow_stack: &gtk::Stack,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
    review_button: &gtk::Button,
    output_pattern: &str,
) {
    review_button.set_sensitive(true);
    review_button.set_label("Review Plan");

    let info_execution = match preflight_result {
        Ok(value) => value,
        Err(error_text) => {
            set_status_error(status_label, "Preflight failed");
            append_structured_log(log_buffer, "hide", LogLevel::Error, &error_text);
            return;
        }
    };

    if info_execution.exit_code != Some(0) {
        set_status_error(status_label, "Preflight failed");
        append_structured_log(
            log_buffer,
            "hide",
            LogLevel::Error,
            "preflight command returned a non-zero exit code",
        );
        for line_text in info_execution.stderr_text.trim_end().lines() {
            append_structured_log(log_buffer, "hide", LogLevel::Error, line_text);
        }
        return;
    }

    let preflight_plan = match parse_hide_preflight_json(&info_execution) {
        Ok(value) => value,
        Err(error_text) => {
            set_status_error(status_label, "Preflight JSON parse failed");
            append_structured_log(log_buffer, "hide", LogLevel::Error, &error_text.to_string());
            return;
        }
    };

    update_output_plan_view(output_plan_view, &preflight_plan, output_pattern);
    set_status_ready(status_label);
    workflow_stack.set_visible_child_name("review");
}

fn update_output_plan_view(
    output_plan_view: &OutputPlanView,
    preflight_plan: &HidePreflightPlan,
    output_pattern: &str,
) {
    if preflight_plan.fits_single {
        update_single_output_plan(output_plan_view, preflight_plan);
        output_plan_view.mode_stack.set_visible_child_name("single");
        output_plan_view.cta_button.set_label("Create Image");
        return;
    }

    update_split_output_plan(output_plan_view, preflight_plan, output_pattern);
    output_plan_view.mode_stack.set_visible_child_name("split");
    output_plan_view
        .cta_button
        .set_label(&format!("Create {} Images", preflight_plan.required_shards));
}

fn update_split_output_plan(
    output_plan_view: &OutputPlanView,
    preflight_plan: &HidePreflightPlan,
    output_pattern: &str,
) {
    let required_shards = preflight_plan.required_shards.max(1u64);
    output_plan_view
        .split
        .result_label
        .set_text(&format!("{required_shards} images will be created"));
    output_plan_view
        .split
        .payload_size_label
        .set_text(&format_file_size(preflight_plan.payload_bytes));
    output_plan_view
        .split
        .capacity_label
        .set_text(&format_file_size(preflight_plan.max_payload_per_shard));
    output_plan_view
        .split
        .images_required_label
        .set_text(&required_shards.to_string());
    output_plan_view.split.notice_title_label.set_text(&format!(
        "All {required_shards} images are required to extract"
    ));
    output_plan_view
        .split
        .file_count_label
        .set_text(&format!("{required_shards} files will be generated:"));

    let visible_rows = usize::try_from(required_shards.min(4u64)).unwrap_or(4usize);
    for (index, row) in output_plan_view.split.file_rows.iter().enumerate() {
        row.set_visible(index < visible_rows);
    }
    for (index, name_label) in output_plan_view.split.file_name_labels.iter().enumerate() {
        let display_index = index + 1;
        name_label.set_text(&preview_name_from_pattern(output_pattern, display_index));
    }
    for (index, size_label) in output_plan_view.split.file_size_labels.iter().enumerate() {
        let shard_size = estimate_shard_payload_size(preflight_plan, index);
        size_label.set_text(&format!("~{}", format_file_size(shard_size)));
    }
}

fn update_single_output_plan(
    output_plan_view: &OutputPlanView,
    preflight_plan: &HidePreflightPlan,
) {
    output_plan_view
        .single
        .result
        .set_text("1 image will be created");
    output_plan_view
        .single
        .payload_size
        .set_text(&format_file_size(preflight_plan.payload_bytes));
    output_plan_view
        .single
        .capacity
        .set_text(&format_file_size(preflight_plan.max_payload_per_shard));
    output_plan_view.single.images_required.set_text("1");
    output_plan_view.single.output_size.set_text(&format!(
        "~{}",
        format_file_size(preflight_plan.payload_bytes)
    ));
}

fn estimate_shard_payload_size(preflight_plan: &HidePreflightPlan, index: usize) -> u64 {
    let shard_start = u64::try_from(index)
        .unwrap_or(u64::MAX)
        .saturating_mul(preflight_plan.max_payload_per_shard);
    if shard_start >= preflight_plan.payload_bytes {
        return 0u64;
    }
    let remaining_payload = preflight_plan.payload_bytes - shard_start;
    remaining_payload.min(preflight_plan.max_payload_per_shard)
}

fn set_status_pending(status_label: &gtk::Label, status_text: &str) {
    status_label.set_markup(&format!(
        "<span foreground='#f59e0b'>●</span>  {}",
        gtk::glib::markup_escape_text(status_text)
    ));
}

fn set_status_ready(status_label: &gtk::Label) {
    status_label.set_markup("<span foreground='#10b981'>●</span>  Ready");
}

fn set_status_error(status_label: &gtk::Label, status_text: &str) {
    status_label.set_markup(&format!(
        "<span foreground='#ef4444'>●</span>  {}",
        gtk::glib::markup_escape_text(status_text)
    ));
}
