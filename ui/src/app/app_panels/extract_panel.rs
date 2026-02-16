use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use super::{
    build_feature_badge_row, build_passphrase_text_entry, build_step_heading, build_step_revealer,
};
use crate::app::app_fields::build_file_field_row;
use crate::app::app_types::ExtractPanel;
use crate::command_builder::default_extract_output_path;

// Build extract controls used by the extract tab
pub fn build_extract_panel(
    window: &gtk::ApplicationWindow,
    log_buffer: &gtk::TextBuffer,
) -> ExtractPanel {
    // File pickers are reused so behavior matches hide panel interactions
    let input_field = build_file_field_row(
        window,
        log_buffer,
        "Stego Image",
        "select stego image to decode",
        gtk::FileChooserAction::Open,
    );
    let output_field = build_file_field_row(
        window,
        log_buffer,
        "Recovered Output",
        "choose destination path for recovered payload",
        gtk::FileChooserAction::Save,
    );
    output_field
        .path_entry
        .set_text(&default_extract_output_path());

    // Extract uses one passphrase field since there is no confirmation step
    let passphrase_text_entry = build_passphrase_text_entry();
    passphrase_text_entry.set_placeholder_text(Some("enter passphrase used during hide"));

    let run_button = gtk::Button::with_label("Extract + Decrypt Payload");
    run_button.add_css_class("action");
    run_button.add_css_class("primary-cta");

    ExtractPanel {
        input_field,
        output_field,
        passphrase_text_entry,
        run_button,
    }
}

// Assemble extract card with simplified required inputs
pub fn assemble_extract_card(extract_panel: &ExtractPanel) -> gtk::Box {
    let panel_box = gtk::Box::new(gtk::Orientation::Vertical, 12);
    panel_box.add_css_class("workflow-panel");

    let title = gtk::Label::new(Some("Extract Payload from Stego Image"));
    title.add_css_class("section-title");
    title.set_xalign(0.0);

    let caption = gtk::Label::new(Some(
        "Select image, choose output path, and enter passphrase",
    ));
    caption.add_css_class("section-caption");
    caption.set_xalign(0.0);

    // Extraction summary badges describe expected outcomes before running
    let badge_row = build_feature_badge_row(&[
        ("auth required", "badge-info"),
        ("generic auth errors", "badge-warning"),
        ("exact payload bytes", "badge-success"),
    ]);

    let step_one_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_one_box.add_css_class("step-card");
    step_one_box.append(&build_step_heading(
        "Step 1",
        "Choose stego image",
        "Input image is decoded and authenticated",
    ));
    step_one_box.append(&extract_panel.input_field.container_box);

    let step_two_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_two_box.add_css_class("step-card");
    step_two_box.append(&build_step_heading(
        "Step 2",
        "Choose recovery output path",
        "Default path is set to Downloads",
    ));
    step_two_box.append(&extract_panel.output_field.container_box);

    let passphrase_label = gtk::Label::new(Some("Passphrase"));
    passphrase_label.add_css_class("section-caption");
    passphrase_label.set_xalign(0.0);

    let step_three_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_three_box.add_css_class("step-card");
    step_three_box.append(&build_step_heading(
        "Step 3",
        "Enter passphrase",
        "Passphrase is passed in-memory and never written to disk",
    ));
    step_three_box.append(&passphrase_label);
    step_three_box.append(&extract_panel.passphrase_text_entry);

    let step_four_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_four_box.add_css_class("step-card");
    step_four_box.append(&build_step_heading(
        "Step 4",
        "Run extract operation",
        "Current build uses deterministic LSB extraction",
    ));
    step_four_box.append(&extract_panel.run_button);

    let step_two_revealer = build_step_revealer(&step_two_box);
    let step_three_revealer = build_step_revealer(&step_three_box);
    let step_four_revealer = build_step_revealer(&step_four_box);

    // Steps are appended in order so reveal logic reads naturally top to bottom
    panel_box.append(&title);
    panel_box.append(&caption);
    panel_box.append(&badge_row);
    panel_box.append(&step_one_box);
    panel_box.append(&step_two_revealer);
    panel_box.append(&step_three_revealer);
    panel_box.append(&step_four_revealer);

    connect_extract_step_visibility(
        extract_panel,
        &step_two_revealer,
        &step_three_revealer,
        &step_four_revealer,
    );

    panel_box
}

fn connect_extract_step_visibility(
    extract_panel: &ExtractPanel,
    step_two_revealer: &gtk::Revealer,
    step_three_revealer: &gtk::Revealer,
    step_four_revealer: &gtk::Revealer,
) {
    // Local clones keep closure captures straightforward
    let input_entry = extract_panel.input_field.path_entry.clone();
    let output_entry = extract_panel.output_field.path_entry.clone();
    let passphrase_text_entry = extract_panel.passphrase_text_entry.clone();

    let step_two_revealer_clone = step_two_revealer.clone();
    let step_three_revealer_clone = step_three_revealer.clone();
    let step_four_revealer_clone = step_four_revealer.clone();

    let update_step_visibility: Rc<dyn Fn()> = Rc::new(move || {
        // Each step unlocks only when prior required fields are non-empty
        let input_ready = !input_entry.text().trim().is_empty();
        let output_ready = !output_entry.text().trim().is_empty();
        let passphrase_ready = !passphrase_text_entry.text().trim().is_empty();

        step_two_revealer_clone.set_reveal_child(input_ready);
        step_three_revealer_clone.set_reveal_child(input_ready && output_ready);
        step_four_revealer_clone.set_reveal_child(input_ready && output_ready && passphrase_ready);
    });

    // Evaluate once so the UI reflects any prefilled defaults immediately
    update_step_visibility();

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        extract_panel
            .input_field
            .path_entry
            .connect_changed(move |_| {
                // Re-evaluate step gating whenever input path changes
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        extract_panel
            .output_field
            .path_entry
            .connect_changed(move |_| {
                // Re-evaluate step gating whenever output path changes
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility;
        extract_panel
            .passphrase_text_entry
            .connect_changed(move |_| {
                // Passphrase changes can unlock or relock final run step
                update_step_visibility_clone();
            });
    }
}
