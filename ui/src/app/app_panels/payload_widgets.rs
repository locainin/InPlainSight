use gtk::prelude::*;
use gtk4 as gtk;

use crate::app::app_types::FileFieldRow;

// Build text payload editor used by hide pasted-text mode
pub fn build_payload_text_view() -> gtk::TextView {
    let payload_text_view = gtk::TextView::new();
    // Entry class aligns text mode look with file-path fields
    payload_text_view.add_css_class("entry");
    payload_text_view.add_css_class("payload-editor");
    payload_text_view.set_monospace(true);
    payload_text_view.set_wrap_mode(gtk::WrapMode::WordChar);
    // Keep the hide form compact even when parent containers expand
    payload_text_view.set_vexpand(false);
    payload_text_view.set_hexpand(true);

    payload_text_view
}

// Build payload stack that switches between file and pasted-text widgets
pub fn build_payload_stack(
    payload_file_field: &FileFieldRow,
    payload_text_view: &gtk::TextView,
) -> gtk::Stack {
    // Text mode labels explain that payload stays in memory-only descriptor
    let payload_text_label = gtk::Label::new(Some("Text Payload"));
    payload_text_label.add_css_class("section-caption");
    payload_text_label.set_xalign(0.0);

    let payload_text_hint = gtk::Label::new(Some(
        "Text is encoded as UTF-8 bytes and passed to the CLI through an in-memory file descriptor",
    ));
    payload_text_hint.add_css_class("section-caption");
    payload_text_hint.set_xalign(0.0);
    // Wrapped captions avoid forcing horizontal scrolling on smaller windows
    payload_text_hint.set_wrap(true);
    payload_text_hint.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    let payload_text_scroll = gtk::ScrolledWindow::new();
    // Fixed minimum height keeps text box usable on smaller windows
    payload_text_scroll.set_min_content_height(130);
    payload_text_scroll.set_vexpand(false);
    payload_text_scroll.add_css_class("payload-scroll");
    payload_text_scroll.set_child(Some(payload_text_view));

    let payload_text_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    payload_text_box.add_css_class("text-mode-box");
    payload_text_box.append(&payload_text_label);
    payload_text_box.append(&payload_text_hint);
    payload_text_box.append(&payload_text_scroll);

    let payload_stack = gtk::Stack::builder()
        .hexpand(true)
        .vexpand(false)
        .hhomogeneous(false)
        .vhomogeneous(false)
        .build();
    // Child names map directly to payload source dropdown values
    payload_stack.add_named(&build_payload_file_card(payload_file_field), Some("file"));
    payload_stack.add_named(&payload_text_box, Some("text"));
    payload_stack.set_visible_child_name("file");

    payload_stack
}

fn build_payload_file_card(payload_file_field: &FileFieldRow) -> gtk::Box {
    let card = gtk::Box::new(gtk::Orientation::Vertical, 8);
    card.add_css_class("payload-file-card");

    let top_row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    top_row.add_css_class("payload-file-top-row");
    top_row.set_hexpand(true);

    let icon = gtk::Label::new(Some("FILE"));
    icon.add_css_class("payload-file-icon");

    let copy_box = gtk::Box::new(gtk::Orientation::Vertical, 3);
    copy_box.set_hexpand(true);

    let title = gtk::Label::new(Some("No payload selected"));
    title.add_css_class("payload-file-title");
    title.set_xalign(0.0);
    title.set_ellipsize(gtk::pango::EllipsizeMode::Middle);

    let meta = gtk::Label::new(Some("Choose the file to hide"));
    meta.add_css_class("payload-file-meta");
    meta.set_xalign(0.0);

    copy_box.append(&title);
    copy_box.append(&meta);

    let path_label = gtk::Label::new(Some("No path selected"));
    path_label.add_css_class("payload-path-label");
    path_label.set_xalign(0.0);
    path_label.set_selectable(true);
    path_label.set_ellipsize(gtk::pango::EllipsizeMode::Middle);

    let reveal = gtk::Expander::new(Some("Path"));
    reveal.add_css_class("payload-path-expander");
    reveal.set_child(Some(&path_label));

    let change_button = gtk::Button::with_label("Choose");
    change_button.add_css_class("secondary");
    change_button.add_css_class("payload-change-button");
    {
        let browse_button = payload_file_field.browse_button.clone();
        change_button.connect_clicked(move |_| browse_button.emit_clicked());
    }

    top_row.append(&icon);
    top_row.append(&copy_box);
    top_row.append(&change_button);

    card.append(&top_row);
    card.append(&reveal);

    wire_payload_file_card(
        &payload_file_field.path_entry,
        &change_button,
        &icon,
        &title,
        &meta,
        &path_label,
    );

    card
}

fn wire_payload_file_card(
    path_entry: &gtk::Entry,
    browse_button: &gtk::Button,
    icon: &gtk::Label,
    title: &gtk::Label,
    meta: &gtk::Label,
    path_label: &gtk::Label,
) {
    let path_entry_for_update = path_entry.clone();
    let path_entry_for_signal = path_entry.clone();
    let browse_button = browse_button.clone();
    let icon = icon.clone();
    let title = title.clone();
    let meta = meta.clone();
    let path_label = path_label.clone();

    let update_card = std::rc::Rc::new(move || {
        let path_text = path_entry_for_update.text().to_string();
        let trimmed_path = path_text.trim();
        if trimmed_path.is_empty() {
            icon.set_text("FILE");
            icon.remove_css_class("payload-file-icon-pdf");
            title.set_text("No payload selected");
            meta.set_text("Choose the file to hide");
            browse_button.set_label("Choose");
            path_entry_for_update.set_tooltip_text(Some("Payload file path"));
            path_label.set_text("No path selected");
            return;
        }

        let path_value = std::path::Path::new(trimmed_path);
        let file_name = path_value
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or(trimmed_path);
        let extension = path_value
            .extension()
            .and_then(|value| value.to_str())
            .unwrap_or("file")
            .to_ascii_uppercase();
        let size_text = std::fs::metadata(path_value).map_or_else(
            |_| "size unknown".to_string(),
            |metadata| format_file_size(metadata.len()),
        );

        if extension == "PDF" {
            icon.set_text("PDF");
            icon.add_css_class("payload-file-icon-pdf");
        } else {
            icon.set_text(extension.as_str());
            icon.remove_css_class("payload-file-icon-pdf");
        }

        title.set_text(file_name);
        meta.set_text(&format!("{extension}  *  {size_text}"));
        browse_button.set_label("Change");
        path_entry_for_update.set_tooltip_text(Some(trimmed_path));
        path_label.set_text(trimmed_path);
    });

    update_card();
    path_entry_for_signal.connect_changed(move |_| update_card());
}

fn format_file_size(byte_count: u64) -> String {
    const KIB: u64 = 1024;
    const MIB: u64 = KIB * 1024;
    const GIB: u64 = MIB * 1024;

    if byte_count >= GIB {
        format_decimal_unit(byte_count, GIB, "GB")
    } else if byte_count >= MIB {
        format_decimal_unit(byte_count, MIB, "MB")
    } else if byte_count >= KIB {
        format_decimal_unit(byte_count, KIB, "KB")
    } else {
        format!("{byte_count} B")
    }
}

fn format_decimal_unit(byte_count: u64, unit_size: u64, unit_label: &str) -> String {
    let whole = byte_count / unit_size;
    let decimal = ((byte_count % unit_size) * 10) / unit_size;
    format!("{whole}.{decimal} {unit_label}")
}
