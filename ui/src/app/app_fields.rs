use gtk::prelude::*;
use gtk4 as gtk;

use crate::command_builder::EmbedMethod;

use super::app_logging::{LogLevel, append_structured_log};
use super::app_types::{ExtractInputMode, FileFieldRow, HidePayloadMode, PassphraseMode};
use super::app_ui_helpers::show_info_dialog;

#[derive(Clone)]
struct FileChooserContext {
    window: gtk::ApplicationWindow,
    log_buffer: gtk::TextBuffer,
    path_entry: gtk::Entry,
    title: String,
}

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

// Extract source selector exposes the CLI's --input and --input-dir modes
pub fn build_extract_input_source_dropdown() -> gtk::DropDown {
    let dropdown = gtk::DropDown::from_strings(&["single image", "split folder"]);
    dropdown.set_selected(0);
    dropdown.add_css_class("selector");
    dropdown.set_tooltip_text(Some(
        "Choose one stego image or a folder of split shard images",
    ));
    dropdown
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
        browse_button,
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

pub fn selected_extract_input_mode(input_source_dropdown: &gtk::DropDown) -> ExtractInputMode {
    match input_source_dropdown.selected() {
        1 => ExtractInputMode::Folder,
        _ => ExtractInputMode::File,
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
    let chooser_context = FileChooserContext {
        window: window.clone(),
        log_buffer: log_buffer.clone(),
        path_entry: path_entry.clone(),
        title: title_text.to_string(),
    };

    browse_button.connect_clicked(move |_| {
        let file_dialog = gtk::FileDialog::builder()
            .title(&chooser_context.title)
            .build();
        match action_type {
            gtk::FileChooserAction::SelectFolder => select_folder(&file_dialog, &chooser_context),
            gtk::FileChooserAction::Save => save_file(&file_dialog, &chooser_context),
            _ => open_file(&file_dialog, &chooser_context),
        }
    });
}

fn select_folder(file_dialog: &gtk::FileDialog, context: &FileChooserContext) {
    let context = context.clone();
    let window = context.window.clone();
    file_dialog.select_folder(
        Some(&window),
        None::<&gtk::gio::Cancellable>,
        move |dialog_result| handle_picker_result(&context, dialog_result, "folder"),
    );
}

fn save_file(file_dialog: &gtk::FileDialog, context: &FileChooserContext) {
    let context = context.clone();
    let window = context.window.clone();
    file_dialog.save(
        Some(&window),
        None::<&gtk::gio::Cancellable>,
        move |dialog_result| handle_picker_result(&context, dialog_result, "file"),
    );
}

fn open_file(file_dialog: &gtk::FileDialog, context: &FileChooserContext) {
    let context = context.clone();
    let window = context.window.clone();
    file_dialog.open(
        Some(&window),
        None::<&gtk::gio::Cancellable>,
        move |dialog_result| handle_picker_result(&context, dialog_result, "file"),
    );
}

fn handle_picker_result(
    context: &FileChooserContext,
    dialog_result: Result<gtk::gio::File, gtk::glib::Error>,
    picker_kind: &str,
) {
    match dialog_result {
        Ok(file_value) => handle_selected_file(context, &file_value, picker_kind),
        Err(error_value) => handle_picker_error(context, &error_value, picker_kind),
    }
}

fn handle_selected_file(
    context: &FileChooserContext,
    file_value: &gtk::gio::File,
    picker_kind: &str,
) {
    if let Some(path_value) = file_value.path() {
        context
            .path_entry
            .set_text(path_value.to_string_lossy().as_ref());
        return;
    }

    append_structured_log(
        &context.log_buffer,
        "ui",
        LogLevel::Error,
        &format!(
            "{picker_kind} picker returned non-local selection for {}",
            context.title
        ),
    );
    show_dialog(
        &context.window,
        "Only local files are supported",
        &format!(
            "{} selection did not produce a local filesystem path",
            context.title
        ),
    );
}

fn handle_picker_error(
    context: &FileChooserContext,
    error_value: &gtk::glib::Error,
    picker_kind: &str,
) {
    if error_value.matches(gtk::gio::IOErrorEnum::Cancelled) {
        return;
    }

    append_structured_log(
        &context.log_buffer,
        "ui",
        LogLevel::Error,
        &format!(
            "{picker_kind} picker failed for {}: {error_value}",
            context.title
        ),
    );
    show_dialog(
        &context.window,
        &format!("{} selection failed", title_case_picker_kind(picker_kind)),
        &format!("{} picker error: {error_value}", context.title),
    );
}

fn show_dialog(parent_window: &gtk::ApplicationWindow, message_text: &str, detail_text: &str) {
    show_info_dialog(parent_window, message_text, detail_text);
}

fn title_case_picker_kind(picker_kind: &str) -> &'static str {
    if picker_kind == "folder" {
        "Folder"
    } else {
        "File"
    }
}
