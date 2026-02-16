use gtk::prelude::*;
use gtk4 as gtk;

// Build text payload editor used by hide pasted-text mode
pub(crate) fn build_payload_text_view() -> gtk::TextView {
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
pub(crate) fn build_payload_stack(
    payload_file_container: &gtk::Box,
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

    let payload_stack = gtk::Stack::builder().hexpand(true).vexpand(false).build();
    // Child names map directly to payload source dropdown values
    payload_stack.add_named(payload_file_container, Some("file"));
    payload_stack.add_named(&payload_text_box, Some("text"));
    payload_stack.set_visible_child_name("file");

    payload_stack
}
