use gtk::prelude::*;
use gtk4 as gtk;

use super::live::wire_live_summary;
use super::types::{DetailLabels, MetricLabels, PlanSummaryPanel, WarningLabels};
use crate::app::app_types::{ExtractPanel, HidePanel};

// Build the right-side inspector panel
//
// This panel summarizes selected inputs and log output. It does not own
// preflight calculation, so it never decides split vs single-image output
pub fn build_plan_summary_panel(
    hide_panel: &HidePanel,
    extract_panel: &ExtractPanel,
    log_buffer: &gtk::TextBuffer,
) -> PlanSummaryPanel {
    // The inspector has a fixed width so the main workflow does not shift
    let container = gtk::Box::new(gtk::Orientation::Vertical, 12);
    container.add_css_class("inspector");
    container.set_width_request(430);

    let (header, state_pill) = build_header();
    let (warning_box, warning_labels) = build_warning_box(state_pill);
    let (metrics_grid, metric_labels) = build_metrics_grid();
    let what_happens = build_what_happens_card();
    let (details_card, detail_labels) = build_details_card();
    let (log_card, clear_log_button) = build_log_card(log_buffer);

    // Cards are appended in the same order the user reads the plan
    container.append(&header);
    container.append(&warning_box);
    container.append(&metrics_grid);
    container.append(&what_happens);
    container.append(&details_card);
    container.append(&log_card);

    // Live updates keep labels current without running the expensive preflight path
    wire_live_summary(
        hide_panel,
        extract_panel,
        log_buffer,
        &warning_labels,
        &detail_labels,
        &metric_labels,
    );

    PlanSummaryPanel {
        container,
        clear_log_button,
    }
}

fn build_header() -> (gtk::Box, gtk::Label) {
    // The state pill communicates readiness, not calculated capacity
    let header = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    let title = gtk::Label::new(Some("Plan Summary"));
    title.add_css_class("inspector-title");
    title.set_xalign(0.0);
    title.set_hexpand(true);

    let state_pill = gtk::Label::new(Some("Estimated"));
    state_pill.add_css_class("state-pill");

    let info_button = gtk::Button::with_label("i");
    info_button.add_css_class("icon-button");
    info_button.set_tooltip_text(Some(
        "Plan updates from selected files and preflight results",
    ));

    header.append(&title);
    header.append(&state_pill);
    header.append(&info_button);

    (header, state_pill)
}

fn build_warning_box(state_pill: gtk::Label) -> (gtk::Box, WarningLabels) {
    // Warning text starts in a neutral pending state and is updated from form readiness
    let warning_box = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    warning_box.add_css_class("plan-warning");
    let warning_icon = gtk::Label::new(Some("!"));
    warning_icon.add_css_class("warning-icon");
    warning_icon.set_valign(gtk::Align::Center);
    let warning_copy = gtk::Box::new(gtk::Orientation::Vertical, 3);
    warning_copy.set_valign(gtk::Align::Center);

    let title = gtk::Label::new(Some("Awaiting preflight"));
    title.add_css_class("warning-title");
    title.set_xalign(0.0);

    let detail = gtk::Label::new(Some(
        "Select files, then run preflight to calculate capacity",
    ));
    detail.add_css_class("warning-detail");
    detail.set_xalign(0.0);
    detail.set_wrap(true);
    detail.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    warning_copy.append(&title);
    warning_copy.append(&detail);
    warning_box.append(&warning_icon);
    warning_box.append(&warning_copy);

    let labels = WarningLabels {
        // Store label handles so live summary updates can avoid rebuilding the card
        state_pill,
        title,
        detail,
    };

    (warning_box, labels)
}

fn build_metrics_grid() -> (gtk::Grid, MetricLabels) {
    // Metrics are compact because the inspector must fit beside every workflow step
    let metrics_grid = gtk::Grid::new();
    metrics_grid.add_css_class("metrics-grid");
    metrics_grid.set_column_spacing(0);

    let (payload_metric, payload_value) = build_metric("Payload", "-");
    let (capacity_metric, capacity_value) = build_metric("Single image capacity", "Run preflight");
    let (shard_metric, shard_value) = build_metric("Images required", "-");
    metrics_grid.attach(&payload_metric, 0, 0, 1, 1);
    metrics_grid.attach(&capacity_metric, 1, 0, 1, 1);
    metrics_grid.attach(&shard_metric, 2, 0, 1, 1);
    (
        metrics_grid,
        MetricLabels {
            // Values are updated by live summary and later preflight-aware code paths
            payload: payload_value,
            capacity: capacity_value,
            shards: shard_value,
        },
    )
}

fn build_what_happens_card() -> gtk::Box {
    // This copy is invariant across single and split output plans
    let what_happens = gtk::Box::new(gtk::Orientation::Vertical, 8);
    what_happens.add_css_class("inspector-card");
    let what_title = gtk::Label::new(Some("What will happen"));
    what_title.add_css_class("inspector-subtitle");
    what_title.set_xalign(0.0);
    let bullet_one = gtk::Label::new(Some("* Payload bytes will be encrypted before embedding"));
    let bullet_two = gtk::Label::new(Some("* Extraction is authenticated and verified"));
    let bullet_three = gtk::Label::new(Some("* Lossless output is required for reliable recovery"));
    for bullet in [&bullet_one, &bullet_two, &bullet_three] {
        // Bullets wrap so long strings do not expand the inspector
        bullet.add_css_class("bullet-label");
        bullet.set_xalign(0.0);
        bullet.set_wrap(true);
        bullet.set_wrap_mode(gtk::pango::WrapMode::WordChar);
    }
    what_happens.append(&what_title);
    what_happens.append(&bullet_one);
    what_happens.append(&bullet_two);
    what_happens.append(&bullet_three);
    what_happens
}

fn build_details_card() -> (gtk::Box, DetailLabels) {
    // Detail rows show the current form selections with middle ellipsis for paths
    let details_grid = gtk::Grid::new();
    details_grid.add_css_class("details-table");
    details_grid.set_column_spacing(0);
    details_grid.set_row_spacing(0);

    let cover_value = gtk::Label::new(Some("-"));
    let payload_value = gtk::Label::new(Some("-"));
    let plan_value = gtk::Label::new(Some("Pending"));
    let output_value = gtk::Label::new(Some("-"));
    let pattern_value = gtk::Label::new(Some("-"));
    let method_value = gtk::Label::new(Some("LSB (default)"));

    add_detail_row(&details_grid, 0, "Cover image", &cover_value);
    add_detail_row(&details_grid, 1, "Payload", &payload_value);
    add_detail_row(&details_grid, 2, "Plan", &plan_value);
    add_detail_row(&details_grid, 3, "Output", &output_value);
    add_detail_row(&details_grid, 4, "File name pattern", &pattern_value);
    add_detail_row(&details_grid, 5, "Method", &method_value);

    let details_card = gtk::Box::new(gtk::Orientation::Vertical, 8);
    details_card.add_css_class("inspector-card");
    details_card.set_visible(true);
    let details_title = gtk::Label::new(Some("Details"));
    details_title.add_css_class("inspector-subtitle");
    details_title.set_xalign(0.0);
    details_card.append(&details_title);
    details_card.append(&details_grid);
    let labels = DetailLabels {
        // These labels are long-lived so signal handlers can update only the values
        cover: cover_value,
        payload: payload_value,
        plan: plan_value,
        output: output_value,
        pattern: pattern_value,
        method: method_value,
    };
    (details_card, labels)
}

fn build_log_card(log_buffer: &gtk::TextBuffer) -> (gtk::Box, gtk::Button) {
    // The log shares the same buffer as execution so errors are visible in context
    let log_header = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    log_header.add_css_class("log-header");
    let log_title = gtk::Label::new(Some("Execution Log"));
    log_title.add_css_class("inspector-subtitle");
    log_title.set_xalign(0.0);
    log_title.set_hexpand(true);
    let clear_log_button = gtk::Button::with_label("Clear");
    clear_log_button.add_css_class("secondary-light");
    log_header.append(&log_title);
    log_header.append(&clear_log_button);

    let log_text_view = gtk::TextView::new();
    log_text_view.set_buffer(Some(log_buffer));
    // The inspector log is read-only and non-focusable to avoid cursor jumps
    log_text_view.set_editable(false);
    log_text_view.set_cursor_visible(false);
    log_text_view.set_focusable(false);
    log_text_view.set_monospace(true);
    log_text_view.add_css_class("light-log-view");

    let log_scroll = gtk::ScrolledWindow::new();
    log_scroll.add_css_class("light-log-scroll");
    log_scroll.set_min_content_height(110);
    log_scroll.set_vexpand(true);
    log_scroll.set_child(Some(&log_text_view));

    let log_card = gtk::Box::new(gtk::Orientation::Vertical, 8);
    log_card.add_css_class("inspector-card");
    log_card.append(&log_header);
    log_card.append(&log_scroll);
    (log_card, clear_log_button)
}

fn build_metric(label_text: &str, value_text: &str) -> (gtk::Box, gtk::Label) {
    // Metric cells return only their value label because labels never change
    let box_widget = gtk::Box::new(gtk::Orientation::Vertical, 4);
    box_widget.add_css_class("metric-cell");
    box_widget.set_hexpand(true);

    let label = gtk::Label::new(Some(label_text));
    label.add_css_class("metric-label");
    label.set_xalign(0.0);
    label.set_wrap(true);
    label.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    let value = gtk::Label::new(Some(value_text));
    value.add_css_class("metric-value");
    value.set_xalign(0.0);

    box_widget.append(&label);
    box_widget.append(&value);
    (box_widget, value)
}

fn add_detail_row(grid: &gtk::Grid, row_index: i32, label_text: &str, value_label: &gtk::Label) {
    // Detail rows use right-aligned values to scan like a property table
    let label = gtk::Label::new(Some(label_text));
    label.add_css_class("detail-key");
    label.set_xalign(0.0);
    label.set_hexpand(true);

    value_label.add_css_class("detail-value");
    value_label.set_xalign(1.0);
    value_label.set_hexpand(true);
    value_label.set_ellipsize(gtk::pango::EllipsizeMode::Middle);

    grid.attach(&label, 0, row_index, 1, 1);
    grid.attach(value_label, 1, row_index, 1, 1);
}
