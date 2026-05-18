use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use super::bindings::{wire_single_output_name_label, wire_single_output_name_preview};
use super::shared::{
    build_change_cover_callout, build_detail_row, build_detail_row_with_label,
    build_lossless_success_notice, build_metric_value_label, build_numbered_next_row,
    build_plan_metric_row, build_single_success_notice,
};
use super::types::SingleOutputPlanWidgets;
use crate::app::app_types::HidePanel;

// Build the left side of the single-image plan view
pub fn build_single_result_column(hide_panel: &HidePanel) -> (gtk::Box, SingleOutputPlanWidgets) {
    // The column combines the calculated result, output path, and cover-change callout
    let column = gtk::Box::new(gtk::Orientation::Vertical, 10);
    column.set_hexpand(true);

    let card = gtk::Box::new(gtk::Orientation::Vertical, 14);
    card.add_css_class("output-plan-card");
    card.add_css_class("output-plan-result-card");
    card.set_hexpand(true);

    let caption = gtk::Label::new(Some("Plan result"));
    caption.add_css_class("compact-field-label");
    caption.set_xalign(0.0);

    let result = gtk::Label::new(Some("1 image will be created"));
    result.add_css_class("output-plan-result");
    result.add_css_class("output-plan-result-success");
    result.set_xalign(0.0);

    // These labels are updated only after the CLI preflight returns real values
    let payload_size = build_metric_value_label("Pending");
    let capacity = build_metric_value_label("Pending");
    let images_required = build_metric_value_label("1");
    images_required.add_css_class("output-plan-result-success");
    let output_size = build_metric_value_label("Pending");
    let notice = build_single_success_notice();

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

    let output_card = gtk::Box::new(gtk::Orientation::Vertical, 12);
    output_card.add_css_class("output-plan-card");
    output_card.add_css_class("output-plan-single-location-card");
    let title = gtk::Label::new(Some("Output location"));
    title.add_css_class("output-plan-card-title");
    title.set_xalign(0.0);
    let hint = gtk::Label::new(Some("Save the generated image to:"));
    hint.add_css_class("muted-label");
    hint.set_xalign(0.0);
    let name_label = gtk::Label::new(Some("Output file name"));
    name_label.add_css_class("compact-field-label");
    name_label.set_xalign(0.0);
    let name_preview = gtk::Entry::new();
    name_preview.add_css_class("entry");
    name_preview.set_editable(false);
    // The preview follows the output field while staying read-only in the plan
    wire_single_output_name_preview(&hide_panel.output_field.path_entry, &name_preview);

    output_card.append(&title);
    output_card.append(&hint);
    output_card.append(&hide_panel.output_field.container_box);
    output_card.append(&name_label);
    output_card.append(&name_preview);

    column.append(&card);
    column.append(&output_card);
    column.append(&build_change_cover_callout());

    let widgets = SingleOutputPlanWidgets {
        result,
        payload_size,
        capacity,
        images_required,
        output_size,
    };
    (column, widgets)
}

// Build the right side of the single-image plan view
pub fn build_single_details_column(
    hide_panel: &HidePanel,
    single_widgets: &SingleOutputPlanWidgets,
) -> gtk::Box {
    // The details card mirrors the final generated image instead of offering choices
    let column = gtk::Box::new(gtk::Orientation::Vertical, 10);
    column.add_css_class("output-plan-single-details-column");
    column.set_hexpand(true);
    column.set_valign(gtk::Align::Start);

    let details = gtk::Box::new(gtk::Orientation::Vertical, 15);
    details.add_css_class("output-plan-card");
    details.add_css_class("output-plan-destination-card");
    details.add_css_class("output-plan-single-details-card");
    details.set_valign(gtk::Align::Start);
    let title = gtk::Label::new(Some("Output image details"));
    title.add_css_class("output-plan-card-title");
    title.set_xalign(0.0);
    let image = build_cover_preview_picture(&hide_panel.cover_field.path_entry);
    let file_name = gtk::Label::new(Some("hidden_payload.png"));
    file_name.add_css_class("output-single-file-name");
    file_name.set_halign(gtk::Align::Center);
    // File name text is derived from the selected output path
    wire_single_output_name_label(&hide_panel.output_field.path_entry, &file_name);
    let badge = gtk::Label::new(Some("Single-image output"));
    badge.add_css_class("green-pill");
    badge.set_halign(gtk::Align::Center);

    details.append(&title);
    details.append(&image);
    details.append(&file_name);
    details.append(&badge);
    details.append(&build_detail_row("Format", "PNG"));
    details.append(&build_detail_row_with_label(
        "Estimated output size",
        &single_widgets.output_size,
    ));
    details.append(&build_detail_row("Color mode", "Truecolor (24-bit)"));
    details.append(&build_detail_row("Compression", "Lossless"));
    details.append(&build_detail_row("Payload status", "Fully embedded"));
    details.append(&build_detail_row("Integrity", "Verified"));
    details.append(&build_lossless_success_notice());

    let next = gtk::Box::new(gtk::Orientation::Vertical, 11);
    next.add_css_class("output-plan-card");
    next.add_css_class("output-plan-next-card");
    let next_title = gtk::Label::new(Some("What happens next"));
    next_title.add_css_class("output-plan-card-title");
    next_title.set_xalign(0.0);
    next.append(&next_title);
    next.append(&build_numbered_next_row(
        "1",
        "The payload will be encrypted before embedding.",
    ));
    next.append(&build_numbered_next_row(
        "2",
        "It will be embedded into this cover image.",
    ));
    next.append(&build_numbered_next_row(
        "3",
        "You'll get one PNG image. Keep it safe to extract later.",
    ));

    column.append(&details);
    column.append(&next);
    column
}

fn build_cover_preview_picture(cover_entry: &gtk::Entry) -> gtk::Stack {
    // Step three reuses the selected cover preview instead of a generic icon
    let stack = gtk::Stack::new();
    stack.add_css_class("output-single-cover-preview-stack");
    stack.set_size_request(190, 136);
    stack.set_halign(gtk::Align::Center);
    stack.set_valign(gtk::Align::Center);
    stack.set_margin_top(10);

    let picture = gtk::Picture::new();
    picture.add_css_class("output-single-cover-preview");
    picture.set_size_request(190, 136);
    picture.set_content_fit(gtk::ContentFit::Cover);
    picture.set_can_shrink(true);
    picture.set_hexpand(true);
    picture.set_vexpand(true);

    let placeholder = gtk::Label::new(Some("Cover image preview"));
    placeholder.add_css_class("output-single-cover-placeholder");
    placeholder.set_halign(gtk::Align::Center);
    placeholder.set_valign(gtk::Align::Center);

    stack.add_named(&picture, Some("picture"));
    stack.add_named(&placeholder, Some("placeholder"));
    stack.set_visible_child_name("placeholder");

    // Match the cover preview from step one so the selected image stays visible in step three
    let cover_entry_for_update = cover_entry.clone();
    let cover_entry_for_signal = cover_entry.clone();
    let stack_for_update = stack.clone();
    let update_preview = Rc::new(move || {
        // GTK Picture can load the same local file path selected on step one
        let path_text = cover_entry_for_update.text().to_string();
        let path_value = std::path::Path::new(path_text.trim());
        if path_value.is_file() {
            // Showing the real cover avoids misleading output previews
            let cover_file = gtk::gio::File::for_path(path_value);
            picture.set_file(Some(&cover_file));
            stack_for_update.set_visible_child_name("picture");
        } else {
            // Empty or stale paths fall back to neutral copy until a cover is selected
            picture.set_file(Option::<&gtk::gio::File>::None);
            stack_for_update.set_visible_child_name("placeholder");
        }
    });
    update_preview();
    cover_entry_for_signal.connect_changed(move |_| update_preview());
    stack
}
