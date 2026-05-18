use gtk::prelude::*;
use gtk4 as gtk;

pub fn show_info_dialog(
    parent_window: &impl IsA<gtk::Window>,
    title_text: &str,
    detail_text: &str,
) {
    show_choice_dialog(parent_window, title_text, detail_text, &["OK"], 0, |_| {});
}

pub fn show_choice_dialog<F>(
    parent_window: &impl IsA<gtk::Window>,
    title_text: &str,
    detail_text: &str,
    button_labels: &[&str],
    default_index: usize,
    on_choice: F,
) where
    F: FnOnce(usize) + 'static,
{
    let dialog = gtk::Window::builder()
        .transient_for(parent_window)
        .modal(true)
        .title(title_text)
        .resizable(false)
        .build();
    dialog.add_css_class("modal-dialog");

    let card = gtk::Box::new(gtk::Orientation::Vertical, 14);
    card.add_css_class("modal-dialog-card");
    card.set_width_request(390);

    let title_label = gtk::Label::new(Some(title_text));
    title_label.add_css_class("modal-dialog-title");
    title_label.set_xalign(0.0);
    title_label.set_wrap(true);
    title_label.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    let detail_label = gtk::Label::new(Some(detail_text));
    detail_label.add_css_class("modal-dialog-detail");
    detail_label.set_xalign(0.0);
    detail_label.set_wrap(true);
    detail_label.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    let action_row = gtk::Box::new(gtk::Orientation::Horizontal, 10);
    action_row.add_css_class("modal-dialog-actions");
    action_row.set_halign(gtk::Align::End);

    let callback = std::rc::Rc::new(std::cell::RefCell::new(Some(on_choice)));
    for (index, label_text) in button_labels.iter().enumerate() {
        let button = gtk::Button::with_label(label_text);
        button.add_css_class(if index == default_index {
            "action"
        } else {
            "secondary"
        });
        let dialog_clone = dialog.clone();
        let callback_clone = callback.clone();
        button.connect_clicked(move |_| {
            dialog_clone.close();
            if let Some(callback_value) = callback_clone.borrow_mut().take() {
                callback_value(index);
            }
        });
        action_row.append(&button);
    }

    card.append(&title_label);
    card.append(&detail_label);
    card.append(&action_row);
    dialog.set_child(Some(&card));
    dialog.present();
}

pub fn set_status_ready(status_label: &gtk::Label, text: &str) {
    status_label.set_markup(&format!("<span foreground='#10b981'>●</span>  {text}"));
}

pub fn set_status_fail(status_label: &gtk::Label, text: &str) {
    status_label.set_markup(&format!("<span foreground='#f43f5e'>●</span>  {text}"));
}

pub fn set_status_pending(status_label: &gtk::Label, text: &str) {
    status_label.set_markup(&format!("<span foreground='#94a3b8'>●</span>  {text}"));
}
