use gtk::prelude::*;
use gtk4 as gtk;

use std::rc::Rc;

use crate::command_builder::EmbedMethod;

use super::app_logging::{LogLevel, append_structured_log};
use super::app_types::{FileFieldRow, HidePayloadMode, PassphraseMode};

// Embedding method options map directly to CLI values
pub fn build_method_dropdown() -> gtk::DropDown {
    // Only expose methods that are fully implemented in the current build
    let dropdown = gtk::DropDown::from_strings(&["lsb"]);
    dropdown.set_selected(0);
    dropdown.add_css_class("selector");
    dropdown.set_tooltip_text(Some("LSB embedding modifies least-significant pixel bits"));
    dropdown
}

// Payload source selector keeps hide mode explicit and discoverable
pub fn build_payload_source_dropdown() -> gtk::DropDown {
    let dropdown = gtk::DropDown::from_strings(&["file", "pasted text"]);
    dropdown.set_selected(0);
    dropdown.add_css_class("selector");
    dropdown.set_tooltip_text(Some(
        "Choose whether payload bytes come from a file or typed text",
    ));
    dropdown
}

// Passphrase source selector keeps default secure typing mode visible
pub fn build_passphrase_source_dropdown() -> gtk::DropDown {
    let dropdown = gtk::DropDown::from_strings(&["typed text", "file"]);
    dropdown.set_selected(0);
    dropdown.add_css_class("selector");
    dropdown.set_tooltip_text(Some(
        "Typed mode keeps passphrase in memory; file mode supports automation",
    ));
    dropdown
}

pub fn build_method_row(method_dropdown: &gtk::DropDown) -> gtk::Box {
    let method_label = gtk::Label::new(Some("Embedding Method (LSB)"));
    method_label.add_css_class("section-caption");
    method_label.set_xalign(0.0);

    let method_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    method_box.append(&method_label);
    method_box.append(method_dropdown);
    method_box
}

pub fn build_payload_source_row(payload_source_dropdown: &gtk::DropDown) -> gtk::Box {
    let source_label = gtk::Label::new(Some("Payload Source"));
    source_label.add_css_class("section-caption");
    source_label.set_xalign(0.0);

    let source_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    source_box.append(&source_label);
    source_box.append(payload_source_dropdown);
    source_box
}

pub fn build_passphrase_source_row(passphrase_source_dropdown: &gtk::DropDown) -> gtk::Box {
    let source_label = gtk::Label::new(Some("Passphrase Source"));
    source_label.add_css_class("section-caption");
    source_label.set_xalign(0.0);

    let source_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    source_box.append(&source_label);
    source_box.append(passphrase_source_dropdown);
    source_box
}

pub fn build_file_field_row(
    window: &gtk::ApplicationWindow,
    log_buffer: &gtk::TextBuffer,
    label_text: &str,
    placeholder_text: &str,
    picker_action: gtk::FileChooserAction,
) -> FileFieldRow {
    // Shared row layout keeps file pickers visually consistent
    let field_label = gtk::Label::new(Some(label_text));
    field_label.add_css_class("section-caption");
    field_label.set_xalign(0.0);

    let path_entry = gtk::Entry::new();
    path_entry.add_css_class("entry");
    path_entry.set_placeholder_text(Some(placeholder_text));
    path_entry.set_hexpand(true);
    path_entry.set_tooltip_text(Some(placeholder_text));

    let browse_button = gtk::Button::with_label("Browse");
    browse_button.add_css_class("secondary");
    browse_button.add_css_class("browse-button");
    browse_button.set_tooltip_text(Some("Open a native file chooser dialog"));

    connect_file_chooser_button(
        window,
        log_buffer,
        &path_entry,
        &browse_button,
        label_text,
        picker_action,
    );

    let row_box = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    row_box.add_css_class("field-row");
    row_box.append(&path_entry);
    row_box.append(&browse_button);

    let container_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    container_box.add_css_class("field-block");
    container_box.append(&field_label);
    container_box.append(&row_box);

    FileFieldRow {
        container_box,
        path_entry,
    }
}

pub fn selected_method(method_dropdown: &gtk::DropDown) -> EmbedMethod {
    let _selected_index = method_dropdown.selected();
    // UI currently offers only LSB because spread mode is not implemented yet
    EmbedMethod::Lsb
}

pub fn selected_payload_mode(payload_source_dropdown: &gtk::DropDown) -> HidePayloadMode {
    match payload_source_dropdown.selected() {
        1 => HidePayloadMode::Text,
        _ => HidePayloadMode::File,
    }
}

pub fn selected_passphrase_mode(passphrase_source_dropdown: &gtk::DropDown) -> PassphraseMode {
    match passphrase_source_dropdown.selected() {
        1 => PassphraseMode::File,
        _ => PassphraseMode::Text,
    }
}

fn connect_file_chooser_button(
    window: &gtk::ApplicationWindow,
    log_buffer: &gtk::TextBuffer,
    path_entry: &gtk::Entry,
    browse_button: &gtk::Button,
    title_text: &str,
    action_type: gtk::FileChooserAction,
) {
    // Clone GTK objects into callback closure owned by button signal
    let window_clone = window.clone();
    let log_buffer_clone = log_buffer.clone();
    let path_entry_clone = path_entry.clone();
    let title_value = title_text.to_string();

    browse_button.connect_clicked(move |_| {
        let file_dialog = gtk::FileDialog::builder().title(&title_value).build();
        let path_entry_response_clone = path_entry_clone.clone();
        let log_buffer_response_clone = log_buffer_clone.clone();

        // Build a reusable dialog closure for "non-local file" cases
        // Rc keeps it callable from the open and save callback branches
        let show_non_local_file_dialog = {
            let window_for_dialog = window_clone.clone();
            let title_for_dialog = title_value.clone();
            Rc::new(move || {
                let dialog = gtk::AlertDialog::builder()
                    .message("Only local files are supported")
                    .detail(format!(
                        "{} selection did not produce a local filesystem path",
                        title_for_dialog
                    ))
                    .build();
                dialog.set_buttons(&["OK"]);
                dialog.set_default_button(0);
                dialog.choose(
                    Some(&window_for_dialog),
                    None::<&gtk::gio::Cancellable>,
                    |_| {},
                );
            })
        };

        match action_type {
            gtk::FileChooserAction::Save => {
                // Save chooser allows creating a new destination path
                let title_for_save = title_value.clone();
                let window_for_save_dialog = window_clone.clone();
                let log_buffer_for_save = log_buffer_response_clone.clone();
                let show_non_local_for_save = show_non_local_file_dialog.clone();

                file_dialog.save(
                    Some(&window_clone),
                    None::<&gtk::gio::Cancellable>,
                    move |dialog_result| match dialog_result {
                        Ok(file_value) => {
                            if let Some(path_value) = file_value.path() {
                                path_entry_response_clone
                                    .set_text(path_value.to_string_lossy().as_ref());
                            } else {
                                append_structured_log(
                                    &log_buffer_for_save,
                                    "ui",
                                    LogLevel::Error,
                                    &format!(
                                        "file picker returned non-local selection for {}",
                                        title_for_save
                                    ),
                                );
                                show_non_local_for_save();
                            }
                        }
                        Err(error_value) => {
                            // Cancel is a normal user action and should stay silent
                            if error_value.matches(gtk::gio::IOErrorEnum::Cancelled) {
                                return;
                            }

                            append_structured_log(
                                &log_buffer_for_save,
                                "ui",
                                LogLevel::Error,
                                &format!(
                                    "file picker failed for {}: {}",
                                    title_for_save, error_value
                                ),
                            );

                            let dialog = gtk::AlertDialog::builder()
                                .message("File selection failed")
                                .detail(format!("{} picker error: {}", title_for_save, error_value))
                                .build();
                            dialog.set_buttons(&["OK"]);
                            dialog.set_default_button(0);
                            dialog.choose(
                                Some(&window_for_save_dialog),
                                None::<&gtk::gio::Cancellable>,
                                |_| {},
                            );
                        }
                    },
                );
            }
            _ => {
                // Open chooser is used for existing input files
                let title_for_open = title_value.clone();
                let window_for_open_dialog = window_clone.clone();
                let log_buffer_for_open = log_buffer_response_clone.clone();
                let show_non_local_for_open = show_non_local_file_dialog.clone();

                file_dialog.open(
                    Some(&window_clone),
                    None::<&gtk::gio::Cancellable>,
                    move |dialog_result| match dialog_result {
                        Ok(file_value) => {
                            if let Some(path_value) = file_value.path() {
                                path_entry_response_clone
                                    .set_text(path_value.to_string_lossy().as_ref());
                            } else {
                                append_structured_log(
                                    &log_buffer_for_open,
                                    "ui",
                                    LogLevel::Error,
                                    &format!(
                                        "file picker returned non-local selection for {}",
                                        title_for_open
                                    ),
                                );
                                show_non_local_for_open();
                            }
                        }
                        Err(error_value) => {
                            if error_value.matches(gtk::gio::IOErrorEnum::Cancelled) {
                                return;
                            }

                            append_structured_log(
                                &log_buffer_for_open,
                                "ui",
                                LogLevel::Error,
                                &format!(
                                    "file picker failed for {}: {}",
                                    title_for_open, error_value
                                ),
                            );

                            let dialog = gtk::AlertDialog::builder()
                                .message("File selection failed")
                                .detail(format!("{} picker error: {}", title_for_open, error_value))
                                .build();
                            dialog.set_buttons(&["OK"]);
                            dialog.set_default_button(0);
                            dialog.choose(
                                Some(&window_for_open_dialog),
                                None::<&gtk::gio::Cancellable>,
                                |_| {},
                            );
                        }
                    },
                );
            }
        }
    });
}
