use gtk::prelude::*;
use gtk4 as gtk;

use super::bindings::wire_output_file_preview;
use super::hero::build_embedded_image;
use super::shared::{
    build_lossless_footer_notice, build_metric_value_label, build_plan_metric_row,
};
use super::types::SplitOutputPlanWidgets;
use crate::app::app_types::HidePanel;

pub fn build_split_output_plan_result_card() -> (gtk::Box, SplitOutputPlanWidgets) {
    let card = gtk::Box::new(gtk::Orientation::Vertical, 14);
    card.add_css_class("output-plan-card");
    card.add_css_class("output-plan-result-card");
    card.set_hexpand(true);

    let caption = gtk::Label::new(Some("Plan result"));
    caption.add_css_class("compact-field-label");
    caption.set_xalign(0.0);

    let result = gtk::Label::new(Some("Run preflight to calculate images"));
    result.add_css_class("output-plan-result");
    result.set_xalign(0.0);
    result.set_wrap(true);

    let payload_size = build_metric_value_label("Pending");
    let capacity = build_metric_value_label("Pending");
    let images_required = build_metric_value_label("Pending");
    images_required.add_css_class("output-plan-result");
    let (notice, notice_title_label) = build_output_requirement_notice();

    card.append(&caption);
    card.append(&result);
    card.append(&build_plan_metric_row(
        "FILE",
        "Original payload size",
        &payload_size,
        None,
    ));
    card.append(&build_plan_metric_row(
        "IMG",
        "Single-image payload capacity",
        &capacity,
        Some("with this cover"),
    ));
    card.append(&build_plan_metric_row(
        "GRID",
        "Images required",
        &images_required,
        None,
    ));
    card.append(&notice);

    let widgets = SplitOutputPlanWidgets {
        result_label: result,
        payload_size_label: payload_size,
        capacity_label: capacity,
        images_required_label: images_required,
        notice_title_label,
        file_count_label: gtk::Label::new(None),
        file_name_labels: Vec::new(),
        file_size_labels: Vec::new(),
        file_rows: Vec::new(),
    };
    (card, widgets)
}

pub fn build_split_output_destination_card(
    hide_panel: &HidePanel,
) -> (
    gtk::Box,
    gtk::Label,
    Vec<gtk::Label>,
    Vec<gtk::Label>,
    Vec<gtk::Box>,
) {
    let card = gtk::Box::new(gtk::Orientation::Vertical, 14);
    card.add_css_class("output-plan-card");
    card.add_css_class("output-plan-destination-card");
    card.set_hexpand(true);
    let (preview, preview_name_labels, preview_size_labels, preview_rows, file_count_label) =
        build_output_files_preview();
    wire_output_file_preview(
        &hide_panel.output_pattern_entry,
        preview_name_labels.clone(),
    );

    let title = gtk::Label::new(Some("Output location"));
    title.add_css_class("output-plan-card-title");
    title.set_xalign(0.0);

    let split_hint = gtk::Label::new(Some("Save the generated images to:"));
    split_hint.add_css_class("muted-label");
    split_hint.set_xalign(0.0);

    let pattern_label = gtk::Label::new(Some("File name pattern"));
    pattern_label.add_css_class("compact-field-label");
    pattern_label.set_xalign(0.0);

    let pattern_hint = gtk::Label::new(Some("Example: hidden_payload_part_0001.png"));
    pattern_hint.add_css_class("muted-label");
    pattern_hint.set_xalign(0.0);

    card.append(&title);
    card.append(&split_hint);
    card.append(&hide_panel.output_dir_field.container_box);
    card.append(&pattern_label);
    card.append(&hide_panel.output_pattern_entry);
    card.append(&pattern_hint);
    card.append(&preview);
    card.append(&build_lossless_footer_notice());

    (
        card,
        file_count_label,
        preview_name_labels,
        preview_size_labels,
        preview_rows,
    )
}

fn build_output_requirement_notice() -> (gtk::Box, gtk::Label) {
    let notice = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    notice.add_css_class("output-plan-notice");

    let icon = gtk::Label::new(Some("i"));
    icon.add_css_class("notice-icon");

    let copy = gtk::Box::new(gtk::Orientation::Vertical, 3);
    let title = gtk::Label::new(Some("All generated images are required to extract"));
    title.add_css_class("output-plan-card-title");
    title.set_xalign(0.0);
    let detail = gtk::Label::new(Some("Keep them together and do not modify or lose any."));
    detail.add_css_class("muted-label");
    detail.set_xalign(0.0);
    detail.set_wrap(true);
    detail.set_wrap_mode(gtk::pango::WrapMode::WordChar);
    copy.append(&title);
    copy.append(&detail);

    notice.append(&icon);
    notice.append(&copy);
    (notice, title)
}

fn build_output_files_preview() -> (
    gtk::Box,
    Vec<gtk::Label>,
    Vec<gtk::Label>,
    Vec<gtk::Box>,
    gtk::Label,
) {
    let preview = gtk::Box::new(gtk::Orientation::Vertical, 6);
    preview.add_css_class("output-files-preview");

    let title = gtk::Label::new(Some("Files to be created"));
    title.add_css_class("output-plan-card-title");
    title.set_xalign(0.0);
    let detail = gtk::Label::new(Some("Preflight will calculate the final count and sizes."));
    detail.add_css_class("muted-label");
    detail.set_xalign(0.0);
    preview.append(&title);
    preview.append(&detail);
    let mut name_labels = Vec::new();
    let mut size_labels = Vec::new();
    let mut row_widgets = Vec::new();
    for file_name in [
        "hidden_payload_part_0001.png",
        "hidden_payload_part_0002.png",
        "hidden_payload_part_0003.png",
        "hidden_payload_part_0004.png",
    ] {
        let (row, name_label, size_label) = build_output_file_preview_row(file_name);
        preview.append(&row);
        name_labels.push(name_label);
        size_labels.push(size_label);
        row_widgets.push(row);
    }
    (preview, name_labels, size_labels, row_widgets, detail)
}

fn build_output_file_preview_row(file_name: &str) -> (gtk::Box, gtk::Label, gtk::Label) {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 10);
    row.add_css_class("output-file-preview-row");
    let icon = build_embedded_image(
        include_bytes!(concat!(
            env!("CARGO_MANIFEST_DIR"),
            "/assets/images/plan-shard.svg"
        )),
        "output-file-preview-icon",
        20,
    );
    let name = gtk::Label::new(Some(file_name));
    name.add_css_class("detail-label");
    name.set_xalign(0.0);
    name.set_hexpand(true);
    let size = gtk::Label::new(Some("pending"));
    size.add_css_class("detail-label");
    size.set_xalign(1.0);
    row.append(&icon);
    row.append(&name);
    row.append(&size);
    (row, name, size)
}
