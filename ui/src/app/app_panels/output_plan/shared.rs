use gtk::prelude::*;
use gtk4 as gtk;

pub fn build_metric_value_label(value_text: &str) -> gtk::Label {
    let label = gtk::Label::new(Some(value_text));
    label.add_css_class("output-plan-metric-value");
    label.set_xalign(1.0);
    label
}

pub fn build_plan_metric_row(
    icon_text: &str,
    label_text: &str,
    value_label: &gtk::Label,
    sublabel_text: Option<&str>,
) -> gtk::Box {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    row.add_css_class("output-plan-metric-row");

    let icon = gtk::Label::new(Some(icon_text));
    icon.add_css_class("output-plan-metric-icon");

    let copy = gtk::Box::new(gtk::Orientation::Vertical, 2);
    copy.set_hexpand(true);
    let label = gtk::Label::new(Some(label_text));
    label.add_css_class("output-plan-metric-label");
    label.set_xalign(0.0);
    copy.append(&label);
    if let Some(sublabel) = sublabel_text {
        let sublabel = gtk::Label::new(Some(sublabel));
        sublabel.add_css_class("muted-label");
        sublabel.set_xalign(0.0);
        copy.append(&sublabel);
    }

    row.append(&icon);
    row.append(&copy);
    row.append(value_label);
    row
}

pub fn build_single_success_notice() -> gtk::Box {
    let notice = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    notice.add_css_class("output-plan-notice");
    notice.add_css_class("output-plan-success-notice");

    let icon = gtk::Label::new(Some("✓"));
    icon.add_css_class("notice-icon");
    icon.add_css_class("notice-icon-success");

    let copy = gtk::Box::new(gtk::Orientation::Vertical, 3);
    let title = gtk::Label::new(Some("Extraction will require this one output image."));
    title.add_css_class("output-plan-card-title");
    title.set_xalign(0.0);
    let detail = gtk::Label::new(Some(
        "Keep it safe. It is the only file needed to recover the hidden data.",
    ));
    detail.add_css_class("muted-label");
    detail.set_xalign(0.0);
    detail.set_wrap(true);
    detail.set_wrap_mode(gtk::pango::WrapMode::WordChar);
    copy.append(&title);
    copy.append(&detail);

    notice.append(&icon);
    notice.append(&copy);
    notice
}

pub fn build_lossless_success_notice() -> gtk::Box {
    let notice = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    notice.add_css_class("output-plan-notice");
    notice.add_css_class("output-plan-success-notice");

    let icon = gtk::Label::new(Some("✓"));
    icon.add_css_class("notice-icon");
    icon.add_css_class("notice-icon-success");

    let detail = gtk::Label::new(Some(
        "Lossless PNG output preserves hidden data without any quality loss.",
    ));
    detail.add_css_class("muted-label");
    detail.set_xalign(0.0);
    detail.set_wrap(true);
    detail.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    notice.append(&icon);
    notice.append(&detail);
    notice
}

pub fn build_lossless_footer_notice() -> gtk::Box {
    let notice = gtk::Box::new(gtk::Orientation::Horizontal, 10);
    notice.add_css_class("output-plan-footer-notice");
    let icon = gtk::Label::new(Some("◆"));
    icon.add_css_class("output-plan-footer-icon");
    let text = gtk::Label::new(Some(
        "Lossless PNG images ensure the hidden data remains intact.",
    ));
    text.add_css_class("muted-label");
    text.set_xalign(0.0);
    text.set_wrap(true);
    notice.append(&icon);
    notice.append(&text);
    notice
}

pub fn build_change_cover_callout() -> gtk::Box {
    let callout = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    callout.add_css_class("output-plan-change-callout");
    let icon = gtk::Label::new(Some("↗"));
    icon.add_css_class("output-plan-change-icon");
    let copy = gtk::Box::new(gtk::Orientation::Vertical, 2);
    let hint = gtk::Label::new(Some(
        "To use a different cover image, go back and choose another one.",
    ));
    hint.add_css_class("muted-label");
    hint.set_xalign(0.0);
    let action = gtk::Label::new(Some("Change cover image  ›"));
    action.add_css_class("output-plan-link-label");
    action.set_xalign(0.0);
    copy.append(&hint);
    copy.append(&action);
    callout.append(&icon);
    callout.append(&copy);
    callout
}

pub fn build_detail_row(label_text: &str, value_text: &str) -> gtk::Box {
    let value = gtk::Label::new(Some(value_text));
    value.add_css_class("detail-label");
    value.set_xalign(1.0);
    build_detail_row_with_label(label_text, &value)
}

pub fn build_detail_row_with_label(label_text: &str, value_label: &gtk::Label) -> gtk::Box {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 10);
    row.add_css_class("output-plan-detail-row");
    let label = gtk::Label::new(Some(label_text));
    label.add_css_class("muted-label");
    label.set_xalign(0.0);
    label.set_hexpand(true);
    row.append(&label);
    row.append(value_label);
    row
}

pub fn build_numbered_next_row(number_text: &str, detail_text: &str) -> gtk::Box {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 10);
    row.add_css_class("output-plan-next-row");
    let number = gtk::Label::new(Some(number_text));
    number.add_css_class("output-plan-next-number");
    let detail = gtk::Label::new(Some(detail_text));
    detail.add_css_class("muted-label");
    detail.set_xalign(0.0);
    detail.set_wrap(true);
    detail.set_wrap_mode(gtk::pango::WrapMode::WordChar);
    row.append(&number);
    row.append(&detail);
    row
}
