use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use super::build_passphrase_text_entry;
use crate::app::app_fields::{
    build_extract_input_source_dropdown, build_file_field_row, selected_extract_input_mode,
};
use crate::app::app_types::{ExtractInputMode, ExtractPanel};
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
    let input_dir_field = build_file_field_row(
        window,
        log_buffer,
        "Split Folder",
        "select folder containing all shard images",
        gtk::FileChooserAction::SelectFolder,
    );
    let input_source_dropdown = build_extract_input_source_dropdown();
    let input_stack =
        build_extract_input_stack(&input_field.container_box, &input_dir_field.container_box);
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
        input_dir_field,
        input_source_dropdown,
        input_stack,
        output_field,
        passphrase_text_entry,
        run_button,
    }
}

// Assemble extract card with simplified required inputs
pub fn assemble_extract_card(extract_panel: &ExtractPanel) -> gtk::Box {
    let panel_box = gtk::Box::new(gtk::Orientation::Vertical, 14);
    panel_box.add_css_class("workflow-panel");

    panel_box.append(&build_extract_stepper());

    let step_one_box = build_extract_section(
        "1. Choose stego input",
        "Use one image or a folder containing all split shard images",
        &build_extract_source_section(extract_panel),
    );

    let step_two_box = build_extract_section(
        "2. Choose recovery output path",
        "Default path is set to Downloads",
        &extract_panel.output_field.container_box,
    );

    let passphrase_label = gtk::Label::new(Some("Passphrase"));
    passphrase_label.add_css_class("compact-field-label");
    passphrase_label.set_xalign(0.0);

    let step_three_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_three_box.add_css_class("light-section");
    step_three_box.append(&numbered_title("3", "Enter passphrase"));
    step_three_box.append(&passphrase_label);
    step_three_box.append(&extract_panel.passphrase_text_entry);

    let step_four_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_four_box.add_css_class("light-section");
    step_four_box.append(&numbered_title("4", "Run extract operation"));
    step_four_box.append(&extract_panel.run_button);

    panel_box.append(&step_one_box);
    panel_box.append(&step_two_box);
    panel_box.append(&step_three_box);
    panel_box.append(&step_four_box);
    connect_extract_step_visibility(extract_panel);

    panel_box
}

fn build_extract_stepper() -> gtk::Box {
    let stepper = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    stepper.add_css_class("top-stepper");
    for (index, label_text) in ["Select Input", "Authenticate", "Recover", "Done"]
        .iter()
        .enumerate()
    {
        let item_box = gtk::Box::new(gtk::Orientation::Vertical, 5);
        item_box.add_css_class("stepper-item");
        item_box.set_hexpand(true);
        let number_label = gtk::Label::new(Some(&(index + 1).to_string()));
        number_label.add_css_class("stepper-number");
        if index == 0 {
            number_label.add_css_class("stepper-number-active");
        }
        let text_label = gtk::Label::new(Some(label_text));
        text_label.add_css_class("stepper-label");
        item_box.append(&number_label);
        item_box.append(&text_label);
        stepper.append(&item_box);
    }
    stepper
}

fn build_extract_input_stack(input_file_box: &gtk::Box, input_dir_box: &gtk::Box) -> gtk::Stack {
    let input_stack = gtk::Stack::builder().hexpand(true).vexpand(false).build();
    input_stack.add_named(input_file_box, Some("file"));
    input_stack.add_named(input_dir_box, Some("folder"));
    input_stack.set_visible_child_name("file");
    input_stack
}

fn build_extract_source_section(extract_panel: &ExtractPanel) -> gtk::Box {
    let source_box = gtk::Box::new(gtk::Orientation::Vertical, 8);
    let source_label = gtk::Label::new(Some("Input source"));
    source_label.add_css_class("compact-field-label");
    source_label.set_xalign(0.0);
    source_box.append(&source_label);
    source_box.append(&extract_panel.input_source_dropdown);
    source_box.append(&extract_panel.input_stack);

    {
        let input_stack_clone = extract_panel.input_stack.clone();
        extract_panel
            .input_source_dropdown
            .connect_selected_notify(
                move |dropdown| match selected_extract_input_mode(dropdown) {
                    ExtractInputMode::Folder => input_stack_clone.set_visible_child_name("folder"),
                    ExtractInputMode::File => input_stack_clone.set_visible_child_name("file"),
                },
            );
    }

    source_box
}

fn build_extract_section(title_text: &str, caption_text: &str, content: &gtk::Box) -> gtk::Box {
    let section = gtk::Box::new(gtk::Orientation::Vertical, 8);
    section.add_css_class("light-section");
    let title = gtk::Label::new(Some(title_text));
    title.add_css_class("light-section-title");
    title.set_xalign(0.0);
    let caption = gtk::Label::new(Some(caption_text));
    caption.add_css_class("muted-label");
    caption.set_xalign(0.0);
    section.append(&title);
    section.append(&caption);
    section.append(content);
    section
}

fn numbered_title(number_text: &str, title_text: &str) -> gtk::Label {
    let title_label = gtk::Label::new(Some(&format!("{number_text}. {title_text}")));
    title_label.add_css_class("light-section-title");
    title_label.set_xalign(0.0);
    title_label
}

fn connect_extract_step_visibility(extract_panel: &ExtractPanel) {
    // Local clones keep closure captures straightforward
    let input_entry = extract_panel.input_field.path_entry.clone();
    let input_dir_entry = extract_panel.input_dir_field.path_entry.clone();
    let input_source_dropdown = extract_panel.input_source_dropdown.clone();
    let output_entry = extract_panel.output_field.path_entry.clone();
    let passphrase_text_entry = extract_panel.passphrase_text_entry.clone();

    let run_button = extract_panel.run_button.clone();

    let update_step_visibility: Rc<dyn Fn()> = Rc::new(move || {
        // Each step unlocks only when prior required fields are non-empty
        let input_ready = match selected_extract_input_mode(&input_source_dropdown) {
            ExtractInputMode::File => !input_entry.text().trim().is_empty(),
            ExtractInputMode::Folder => !input_dir_entry.text().trim().is_empty(),
        };
        let output_ready = !output_entry.text().trim().is_empty();
        let passphrase_ready = !passphrase_text_entry.text().trim().is_empty();

        run_button.set_sensitive(input_ready && output_ready && passphrase_ready);
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
        let update_step_visibility_clone = update_step_visibility.clone();
        extract_panel
            .input_dir_field
            .path_entry
            .connect_changed(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        extract_panel
            .input_source_dropdown
            .connect_selected_notify(move |_| {
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
