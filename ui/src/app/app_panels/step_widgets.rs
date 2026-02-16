use gtk::prelude::*;
use gtk4 as gtk;

// Build reusable step heading with chip, title, and caption
pub(crate) fn build_step_heading(
    step_label_text: &str,
    title_text: &str,
    caption_text: &str,
) -> gtk::Box {
    // Step chip highlights workflow order visually
    let step_label = gtk::Label::new(Some(step_label_text));
    step_label.add_css_class("step-chip");
    step_label.set_xalign(0.0);

    let title_label = gtk::Label::new(Some(title_text));
    title_label.add_css_class("section-title");
    title_label.set_xalign(0.0);

    let caption_label = gtk::Label::new(Some(caption_text));
    caption_label.add_css_class("section-caption");
    caption_label.set_xalign(0.0);
    // Captions can be long, so wrapping keeps layouts stable on smaller windows
    caption_label.set_wrap(true);
    caption_label.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    let heading_box = gtk::Box::new(gtk::Orientation::Vertical, 3);
    // Heading groups chip, title, and caption into one reusable block
    heading_box.append(&step_label);
    heading_box.append(&title_label);
    heading_box.append(&caption_label);

    heading_box
}

// Build compact feature badge used in panel summary rows
pub(crate) fn build_feature_badge(badge_text: &str, css_class_name: &str) -> gtk::Label {
    let badge_label = gtk::Label::new(Some(badge_text));
    badge_label.add_css_class("feature-badge");
    badge_label.add_css_class(css_class_name);
    badge_label.set_xalign(0.0);
    badge_label
}

// Build reusable horizontal row of feature badges
pub(crate) fn build_feature_badge_row(badge_values: &[(&str, &str)]) -> gtk::Box {
    let badge_row = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    badge_row.add_css_class("feature-badge-row");

    // Badge order is preserved so the most important guarantees appear first
    for (badge_text, css_class_name) in badge_values {
        badge_row.append(&build_feature_badge(badge_text, css_class_name));
    }

    badge_row
}

// Build a step revealer used for progressive workflow sections
pub(crate) fn build_step_revealer(step_box: &gtk::Box) -> gtk::Revealer {
    let step_revealer = gtk::Revealer::new();
    // Slide animation keeps step transitions visible without abrupt jumps
    step_revealer.set_transition_type(gtk::RevealerTransitionType::SlideDown);
    step_revealer.set_transition_duration(180);
    step_revealer.set_reveal_child(false);
    step_revealer.set_child(Some(step_box));
    step_revealer
}
