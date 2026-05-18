use gtk::prelude::*;
use gtk4 as gtk;

use super::super::passphrase_widgets::{build_hide_passphrase_stack, build_passphrase_text_entry};
use super::super::payload_widgets::{build_payload_stack, build_payload_text_view};
use crate::app::app_fields::{
    build_file_field_row, build_method_dropdown, build_passphrase_source_dropdown,
    build_payload_source_dropdown,
};
use crate::app::app_types::{FileFieldRow, HidePanel};
use crate::command_builder::default_hide_output_path;

pub fn build_hide_panel(
    window: &gtk::ApplicationWindow,
    log_buffer: &gtk::TextBuffer,
) -> HidePanel {
    let cover_field = build_file_field_row(
        window,
        log_buffer,
        "Cover Image",
        "select .png / .jxl / .bmp / .ppm / .jpg / .jpeg / .webp cover file",
        gtk::FileChooserAction::Open,
    );
    let payload_file_field = build_file_field_row(
        window,
        log_buffer,
        "Payload File",
        "select payload file to hide",
        gtk::FileChooserAction::Open,
    );
    let (output_field, output_dir_field, output_pattern_entry) =
        build_output_fields(window, log_buffer);
    let passphrase_file_field = build_file_field_row(
        window,
        log_buffer,
        "Passphrase File",
        "select text file containing passphrase",
        gtk::FileChooserAction::Open,
    );

    let payload_source_dropdown = build_payload_source_dropdown();
    let payload_text_view = build_payload_text_view();
    let payload_stack = build_payload_stack(&payload_file_field, &payload_text_view);

    let passphrase_source_dropdown = build_passphrase_source_dropdown();
    let passphrase_text_entry = build_passphrase_text_entry();
    let passphrase_confirm_entry = build_passphrase_text_entry();
    let passphrase_stack = build_hide_passphrase_stack(
        &passphrase_file_field.container_box,
        &passphrase_text_entry,
        &passphrase_confirm_entry,
    );

    // Keep payload stack in sync with selected source
    let payload_stack_clone = payload_stack.clone();
    payload_source_dropdown.connect_selected_notify(move |dropdown| match dropdown.selected() {
        1 => payload_stack_clone.set_visible_child_name("text"),
        _ => payload_stack_clone.set_visible_child_name("file"),
    });

    // Keep passphrase stack in sync with selected source
    let passphrase_stack_clone = passphrase_stack.clone();
    passphrase_source_dropdown.connect_selected_notify(move |dropdown| match dropdown.selected() {
        1 => passphrase_stack_clone.set_visible_child_name("file"),
        _ => passphrase_stack_clone.set_visible_child_name("text"),
    });

    let method_dropdown = build_method_dropdown();

    let (reset_button, run_button) = build_action_buttons(&payload_source_dropdown);

    HidePanel {
        cover_field,
        payload_file_field,
        output_field,
        output_dir_field,
        output_pattern_entry,
        passphrase_file_field,
        payload_source_dropdown,
        payload_stack,
        payload_text_view,
        passphrase_source_dropdown,
        passphrase_stack,
        passphrase_text_entry,
        passphrase_confirm_entry,
        method_dropdown,
        reset_button,
        run_button,
    }
}

fn build_output_fields(
    window: &gtk::ApplicationWindow,
    log_buffer: &gtk::TextBuffer,
) -> (FileFieldRow, FileFieldRow, gtk::Entry) {
    let output_field = build_file_field_row(
        window,
        log_buffer,
        "Output Image",
        "defaulted automatically, edit if needed",
        gtk::FileChooserAction::Save,
    );
    output_field
        .path_entry
        .set_text(&default_hide_output_path());

    let output_dir_field = build_file_field_row(
        window,
        log_buffer,
        "Output folder",
        "choose folder for split shard images",
        gtk::FileChooserAction::SelectFolder,
    );
    output_dir_field
        .path_entry
        .set_text(&default_split_output_dir());

    let output_pattern_entry = gtk::Entry::new();
    output_pattern_entry.add_css_class("entry");
    output_pattern_entry.set_hexpand(true);
    output_pattern_entry.set_text("hidden_payload_part_{index}.png");
    output_pattern_entry.set_tooltip_text(Some("Filename pattern preview for split shard outputs"));
    (output_field, output_dir_field, output_pattern_entry)
}

fn build_action_buttons(_payload_source_dropdown: &gtk::DropDown) -> (gtk::Button, gtk::Button) {
    let reset_button = gtk::Button::with_label("Reset");
    reset_button.add_css_class("secondary");

    let run_button = gtk::Button::with_label("Hide Payload");
    run_button.add_css_class("action");
    run_button.add_css_class("primary-cta");
    (reset_button, run_button)
}

pub(super) fn default_split_output_dir() -> String {
    std::env::var("HOME").map_or_else(
        |_| "hidden_payload_images".to_string(),
        |home_path| format!("{home_path}/Downloads/hidden_payload_images"),
    )
}
