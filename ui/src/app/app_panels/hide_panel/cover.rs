use gtk::prelude::*;
use gtk4 as gtk;

use super::super::formatting::format_file_size;
use super::pages::numbered_title;
use crate::app::app_types::HidePanel;

pub(super) fn build_cover_section(hide_panel: &HidePanel) -> gtk::Box {
    let section = gtk::Box::new(gtk::Orientation::Vertical, 10);
    section.add_css_class("light-section");
    section.append(&numbered_title("1", "Select a cover image"));

    let cover_card = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    cover_card.add_css_class("cover-card");
    cover_card.set_hexpand(true);

    let preview_stack = gtk::Stack::new();
    preview_stack.add_css_class("cover-preview-stack");
    preview_stack.set_hexpand(true);

    let preview_placeholder = gtk::Label::new(Some("Select a cover image to preview"));
    preview_placeholder.add_css_class("cover-placeholder");
    preview_placeholder.set_widget_name("cover-preview-placeholder");

    let cover_picture = gtk::Picture::new();
    cover_picture.set_widget_name("cover-preview-picture");
    cover_picture.add_css_class("cover-picture");
    cover_picture.set_content_fit(gtk::ContentFit::Cover);
    cover_picture.set_can_shrink(true);

    preview_stack.add_named(&preview_placeholder, Some("placeholder"));
    preview_stack.add_named(&cover_picture, Some("picture"));
    preview_stack.set_visible_child_name("placeholder");

    let details_box = gtk::Box::new(gtk::Orientation::Vertical, 10);
    details_box.add_css_class("cover-details");
    details_box.set_width_request(300);

    let file_name_label = gtk::Label::new(Some("No cover selected"));
    file_name_label.add_css_class("file-name-title");
    file_name_label.set_widget_name("cover-file-name");
    file_name_label.set_xalign(0.0);

    let meta_label = gtk::Label::new(Some("PNG, JXL, BMP, PPM, JPEG, or WebP"));
    meta_label.add_css_class("muted-label");
    meta_label.set_widget_name("cover-meta");
    meta_label.set_xalign(0.0);

    let size_label = gtk::Label::new(Some("File size: -"));
    size_label.add_css_class("detail-label");
    size_label.set_widget_name("cover-size");
    size_label.set_xalign(0.0);

    let decoded_label = gtk::Label::new(Some("Decoded size: Run preflight"));
    decoded_label.add_css_class("detail-label");
    decoded_label.set_xalign(0.0);

    let lossless_badge = gtk::Label::new(Some("Output: lossless PNG"));
    lossless_badge.add_css_class("green-pill");
    lossless_badge.set_halign(gtk::Align::Start);

    details_box.append(&file_name_label);
    details_box.append(&meta_label);
    details_box.append(&size_label);
    details_box.append(&decoded_label);
    details_box.append(&lossless_badge);

    {
        let cover_entry = hide_panel.cover_field.path_entry.clone();
        let preview_stack_clone = preview_stack.clone();
        let cover_picture_clone = cover_picture;
        let file_name_label_clone = file_name_label;
        let size_label_clone = size_label;
        let meta_label_clone = meta_label;

        let update_cover_preview = move || {
            let path_text = cover_entry.text().to_string();
            let path_value = std::path::Path::new(&path_text);

            if let Ok(file_metadata) = std::fs::metadata(path_value) {
                let file_name = path_value
                    .file_name()
                    .and_then(|value| value.to_str())
                    .unwrap_or("selected cover");
                let extension = path_value
                    .extension()
                    .and_then(|value| value.to_str())
                    .map_or_else(|| "IMAGE".to_string(), str::to_ascii_uppercase);

                file_name_label_clone.set_text(file_name);
                size_label_clone.set_text(&format!(
                    "File size: {}",
                    format_file_size(file_metadata.len())
                ));
                let input_copy = if matches!(extension.as_str(), "JPG" | "JPEG" | "WEBP") {
                    "lossy input accepted"
                } else {
                    "lossless carrier input"
                };
                meta_label_clone.set_text(&format!("{extension}  *  {input_copy}"));
            } else {
                file_name_label_clone.set_text("No cover selected");
                size_label_clone.set_text("File size: -");
                meta_label_clone.set_text("PNG, JXL, BMP, PPM, JPEG, or WebP");
            }

            if path_value.is_file() {
                let cover_file = gtk::gio::File::for_path(path_value);
                cover_picture_clone.set_file(Some(&cover_file));
                preview_stack_clone.set_visible_child_name("picture");
            } else {
                cover_picture_clone.set_file(Option::<&gtk::gio::File>::None);
                preview_stack_clone.set_visible_child_name("placeholder");
            }
        };

        update_cover_preview();
        hide_panel.cover_field.path_entry.connect_changed(move |_| {
            update_cover_preview();
        });
    }

    cover_card.append(&preview_stack);
    cover_card.append(&details_box);
    section.append(&cover_card);
    section.append(&hide_panel.cover_field.container_box);
    section
}
