use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use super::naming::preview_name_from_pattern;

pub fn wire_single_output_name_preview(output_entry: &gtk::Entry, preview_entry: &gtk::Entry) {
    let output_entry_for_update = output_entry.clone();
    let output_entry_for_signal = output_entry.clone();
    let preview_entry = preview_entry.clone();
    let update_preview = Rc::new(move || {
        let path_text = output_entry_for_update.text().to_string();
        let file_name = std::path::Path::new(path_text.trim())
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("hidden_payload.png");
        preview_entry.set_text(file_name);
    });
    update_preview();
    output_entry_for_signal.connect_changed(move |_| update_preview());
}

pub fn wire_single_output_name_label(output_entry: &gtk::Entry, name_label: &gtk::Label) {
    let output_entry_for_update = output_entry.clone();
    let output_entry_for_signal = output_entry.clone();
    let name_label = name_label.clone();
    let update_preview = Rc::new(move || {
        let path_text = output_entry_for_update.text().to_string();
        let file_name = std::path::Path::new(path_text.trim())
            .file_name()
            .and_then(|value| value.to_str())
            .unwrap_or("hidden_payload.png");
        name_label.set_text(file_name);
    });
    update_preview();
    output_entry_for_signal.connect_changed(move |_| update_preview());
}

pub fn wire_output_file_preview(pattern_entry: &gtk::Entry, preview_name_labels: Vec<gtk::Label>) {
    let pattern_entry_for_update = pattern_entry.clone();
    let pattern_entry_for_signal = pattern_entry.clone();
    let update_preview = std::rc::Rc::new(move || {
        let pattern_text = pattern_entry_for_update.text().to_string();
        for (index, name_label) in preview_name_labels.iter().enumerate() {
            name_label.set_text(&preview_name_from_pattern(&pattern_text, index + 1));
        }
    });
    update_preview();
    pattern_entry_for_signal.connect_changed(move |_| update_preview());
}
