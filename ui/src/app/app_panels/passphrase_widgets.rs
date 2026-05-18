use gtk::prelude::*;
use gtk4 as gtk;

// Build masked passphrase entry with reveal-eye support
pub fn build_passphrase_text_entry() -> gtk::PasswordEntry {
    let entry_widget = gtk::PasswordEntry::new();
    // Shared class keeps typed passphrase visuals consistent across flows
    entry_widget.add_css_class("entry");
    entry_widget.add_css_class("passphrase-entry");
    entry_widget.set_show_peek_icon(true);
    entry_widget.set_hexpand(true);
    entry_widget
}

// Build hide passphrase stack containing typed and file modes
pub fn build_hide_passphrase_stack(
    passphrase_file_container: &gtk::Box,
    passphrase_text_entry: &gtk::PasswordEntry,
    passphrase_confirm_entry: &gtk::PasswordEntry,
) -> gtk::Stack {
    // Labels are created here so both typed fields share panel styling
    let passphrase_text_label = gtk::Label::new(Some("Passphrase"));
    passphrase_text_label.add_css_class("section-caption");
    passphrase_text_label.set_xalign(0.0);

    passphrase_text_entry.set_placeholder_text(Some("enter strong passphrase"));

    let passphrase_confirm_label = gtk::Label::new(Some("Confirm Passphrase"));
    passphrase_confirm_label.add_css_class("section-caption");
    passphrase_confirm_label.set_xalign(0.0);

    passphrase_confirm_entry.set_placeholder_text(Some("re-enter passphrase"));

    let mismatch_label = gtk::Label::new(Some("Passphrases do not match"));
    mismatch_label.add_css_class("inline-error");
    mismatch_label.set_xalign(0.0);
    mismatch_label.set_wrap(true);
    mismatch_label.set_visible(false);

    {
        let passphrase_text_entry_clone = passphrase_text_entry.clone();
        let passphrase_confirm_entry_clone = passphrase_confirm_entry.clone();
        let mismatch_label_clone = mismatch_label.clone();

        // Keep mismatch feedback local to the passphrase widget
        // This avoids spreading UI state across panels and validation code
        let update_mismatch_state = move || {
            let passphrase_text = passphrase_text_entry_clone.text();
            let confirm_text = passphrase_confirm_entry_clone.text();

            // Empty confirm field keeps the UI quiet until a mismatch is possible
            if confirm_text.is_empty() {
                mismatch_label_clone.set_visible(false);
                passphrase_confirm_entry_clone.remove_css_class("field-invalid");
                return;
            }

            // Mismatch state is only meaningful when a passphrase exists
            if passphrase_text.is_empty() {
                mismatch_label_clone.set_visible(false);
                passphrase_confirm_entry_clone.remove_css_class("field-invalid");
                return;
            }

            let matches = passphrase_text.as_str() == confirm_text.as_str();
            mismatch_label_clone.set_visible(!matches);
            if matches {
                passphrase_confirm_entry_clone.remove_css_class("field-invalid");
            } else {
                passphrase_confirm_entry_clone.add_css_class("field-invalid");
            }
        };

        let update_mismatch_state_clone = update_mismatch_state.clone();
        passphrase_text_entry.connect_changed(move |_| {
            update_mismatch_state_clone();
        });

        passphrase_confirm_entry.connect_changed(move |_| {
            update_mismatch_state();
        });
    }

    let typed_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    typed_box.add_css_class("typed-passphrase-box");
    // Typed view contains both passphrase and confirmation entries
    typed_box.append(&passphrase_text_label);
    typed_box.append(passphrase_text_entry);
    typed_box.append(&passphrase_confirm_label);
    typed_box.append(passphrase_confirm_entry);
    typed_box.append(&mismatch_label);

    let stack_widget = gtk::Stack::builder().hexpand(true).vexpand(false).build();
    // Stack child names are wired to dropdown values in hide panel code
    stack_widget.add_named(&typed_box, Some("text"));
    stack_widget.add_named(passphrase_file_container, Some("file"));
    stack_widget.set_visible_child_name("text");

    stack_widget
}
