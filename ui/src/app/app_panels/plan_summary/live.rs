use gtk::prelude::*;
use gtk4 as gtk;

use super::formatting::{basename_or_dash, payload_summary};
use super::types::{DetailLabels, MetricLabels, SummarySources, WarningLabels};
use crate::app::app_types::{ExtractPanel, HidePanel};

pub(super) fn wire_live_summary(
    hide_panel: &HidePanel,
    _extract_panel: &ExtractPanel,
    _log_buffer: &gtk::TextBuffer,
    warning_labels: &WarningLabels,
    detail_labels: &DetailLabels,
    metric_labels: &MetricLabels,
) {
    let sources = capture_summary_sources(hide_panel);
    let update_summary =
        build_summary_update(&sources, warning_labels, detail_labels, metric_labels);

    update_summary();

    connect_summary_entries(
        &update_summary,
        [
            sources.cover_entry,
            sources.payload_entry,
            sources.output_entry,
            sources.output_dir_entry,
            sources.pattern_entry,
        ],
    );
    connect_summary_dropdowns(&update_summary, [sources.method_dropdown]);
}

fn capture_summary_sources(hide_panel: &HidePanel) -> SummarySources {
    SummarySources {
        cover_entry: hide_panel.cover_field.path_entry.clone(),
        payload_entry: hide_panel.payload_file_field.path_entry.clone(),
        output_entry: hide_panel.output_field.path_entry.clone(),
        output_dir_entry: hide_panel.output_dir_field.path_entry.clone(),
        pattern_entry: hide_panel.output_pattern_entry.clone(),
        method_dropdown: hide_panel.method_dropdown.clone(),
    }
}

fn build_summary_update(
    sources: &SummarySources,
    warning_labels: &WarningLabels,
    detail_labels: &DetailLabels,
    metric_labels: &MetricLabels,
) -> std::rc::Rc<dyn Fn()> {
    let sources = sources.clone();
    let warning_labels = warning_labels.clone();
    let detail_labels = detail_labels.clone();
    let metric_labels = metric_labels.clone();
    std::rc::Rc::new(move || {
        refresh_summary(&sources, &warning_labels, &detail_labels, &metric_labels);
    })
}

fn refresh_summary(
    sources: &SummarySources,
    warning_labels: &WarningLabels,
    detail_labels: &DetailLabels,
    metric_labels: &MetricLabels,
) {
    let cover_path = sources.cover_entry.text().to_string();
    let payload_path = sources.payload_entry.text().to_string();
    let payload_summary_text = payload_summary(&payload_path);

    update_detail_labels(sources, detail_labels, &cover_path, &payload_summary_text);
    update_metric_labels(metric_labels, &cover_path, &payload_summary_text);
    update_warning_labels(warning_labels, &cover_path, &payload_path);
}

fn update_detail_labels(
    sources: &SummarySources,
    detail_labels: &DetailLabels,
    cover_path: &str,
    payload_summary_text: &str,
) {
    detail_labels.cover.set_text(&basename_or_dash(cover_path));
    detail_labels.payload.set_text(payload_summary_text);
    detail_labels.plan.set_text("Automatic preflight");
    detail_labels
        .output
        .set_text(sources.output_entry.text().as_str());
    detail_labels
        .pattern
        .set_text(sources.pattern_entry.text().as_str());
    let _method_index = sources.method_dropdown.selected();
    detail_labels.method.set_text("LSB (default)");
}

fn update_metric_labels(
    metric_labels: &MetricLabels,
    cover_path: &str,
    payload_summary_text: &str,
) {
    metric_labels.payload.set_text(payload_summary_text);
    metric_labels
        .capacity
        .set_text(if cover_path.trim().is_empty() {
            "Select cover"
        } else {
            "Needs preflight"
        });
    metric_labels.shards.set_text("Preflight decides");
}

fn update_warning_labels(warning_labels: &WarningLabels, cover_path: &str, payload_path: &str) {
    if !cover_path.trim().is_empty() && !payload_path.trim().is_empty() {
        warning_labels.state_pill.set_text("Ready");
        warning_labels.title.set_text("Ready for preflight");
        warning_labels
            .detail
            .set_text("Run preflight to calculate exact capacity and image count");
    } else {
        warning_labels.state_pill.set_text("Pending");
        warning_labels.title.set_text("Awaiting files");
        warning_labels
            .detail
            .set_text("Choose a cover image and payload before capacity can be calculated");
    }
}

fn connect_summary_entries(update_summary: &std::rc::Rc<dyn Fn()>, entries: [gtk::Entry; 5]) {
    for entry in entries {
        let update_summary_clone = update_summary.clone();
        entry.connect_changed(move |_| update_summary_clone());
    }
}

fn connect_summary_dropdowns(
    update_summary: &std::rc::Rc<dyn Fn()>,
    dropdowns: [gtk::DropDown; 1],
) {
    for dropdown in dropdowns {
        let update_summary_clone = update_summary.clone();
        dropdown.connect_selected_notify(move |_| update_summary_clone());
    }
}
