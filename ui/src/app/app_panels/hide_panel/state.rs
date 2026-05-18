use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use super::builder::default_split_output_dir;
use crate::app::app_fields::{selected_passphrase_mode, selected_payload_mode};
use crate::app::app_types::{HidePanel, HidePayloadMode, PassphraseMode};
use crate::command_builder::default_hide_output_path;

pub(super) fn connect_hide_step_visibility(hide_panel: &HidePanel) {
    let cover_entry = hide_panel.cover_field.path_entry.clone();
    let payload_file_entry = hide_panel.payload_file_field.path_entry.clone();
    let output_entry = hide_panel.output_field.path_entry.clone();
    let output_dir_entry = hide_panel.output_dir_field.path_entry.clone();
    let output_pattern_entry = hide_panel.output_pattern_entry.clone();
    let passphrase_file_entry = hide_panel.passphrase_file_field.path_entry.clone();
    let passphrase_text_entry = hide_panel.passphrase_text_entry.clone();
    let passphrase_confirm_entry = hide_panel.passphrase_confirm_entry.clone();
    let payload_source_dropdown = hide_panel.payload_source_dropdown.clone();
    let payload_text_view = hide_panel.payload_text_view.clone();
    let passphrase_source_dropdown = hide_panel.passphrase_source_dropdown.clone();
    let run_button = hide_panel.run_button.clone();

    let update_step_visibility: Rc<dyn Fn()> = Rc::new(move || {
        let cover_ready = !cover_entry.text().trim().is_empty();

        let payload_ready = match selected_payload_mode(&payload_source_dropdown) {
            HidePayloadMode::File => !payload_file_entry.text().trim().is_empty(),
            HidePayloadMode::Text => text_payload_has_content(&payload_text_view),
        };

        let output_ready = !output_entry.text().trim().is_empty()
            && !output_dir_entry.text().trim().is_empty()
            && output_pattern_has_index(output_pattern_entry.text().as_str());

        let passphrase_ready = match selected_passphrase_mode(&passphrase_source_dropdown) {
            PassphraseMode::File => !passphrase_file_entry.text().trim().is_empty(),
            PassphraseMode::Text => {
                let passphrase_text = passphrase_text_entry.text();
                let confirm_text = passphrase_confirm_entry.text();
                !passphrase_text.trim().is_empty()
                    && passphrase_text.as_str() == confirm_text.as_str()
            }
        };

        run_button.set_sensitive(cover_ready && payload_ready && output_ready && passphrase_ready);
    });

    // Evaluate once so the UI reflects any prefilled defaults immediately
    update_step_visibility();
    connect_text_inputs_for_step_visibility(hide_panel, &update_step_visibility);
    connect_dropdowns_for_step_visibility(hide_panel, &update_step_visibility);
    connect_payload_buffer_for_step_visibility(hide_panel, update_step_visibility);
}

fn connect_text_inputs_for_step_visibility(hide_panel: &HidePanel, update: &Rc<dyn Fn()>) {
    for entry in [
        hide_panel.cover_field.path_entry.clone(),
        hide_panel.payload_file_field.path_entry.clone(),
        hide_panel.output_field.path_entry.clone(),
        hide_panel.output_dir_field.path_entry.clone(),
        hide_panel.output_pattern_entry.clone(),
        hide_panel.passphrase_file_field.path_entry.clone(),
    ] {
        let update_clone = update.clone();
        entry.connect_changed(move |_| update_clone());
    }

    for entry in [
        hide_panel.passphrase_text_entry.clone(),
        hide_panel.passphrase_confirm_entry.clone(),
    ] {
        let update_clone = update.clone();
        entry.connect_changed(move |_| update_clone());
    }
}

fn connect_dropdowns_for_step_visibility(hide_panel: &HidePanel, update: &Rc<dyn Fn()>) {
    for dropdown in [
        hide_panel.payload_source_dropdown.clone(),
        hide_panel.passphrase_source_dropdown.clone(),
    ] {
        let update_clone = update.clone();
        dropdown.connect_selected_notify(move |_| update_clone());
    }
}

pub(super) fn connect_run_button_copy(hide_panel: &HidePanel) {
    hide_panel.run_button.set_label("Hide Payload");
}

fn connect_payload_buffer_for_step_visibility(hide_panel: &HidePanel, update: Rc<dyn Fn()>) {
    hide_panel
        .payload_text_view
        .buffer()
        .connect_changed(move |_| update());
}

pub(super) fn wire_continue_to_preflight_state(
    hide_panel: &HidePanel,
    continue_button: &gtk::Button,
) {
    let cover_entry = hide_panel.cover_field.path_entry.clone();
    let payload_file_entry = hide_panel.payload_file_field.path_entry.clone();
    let payload_source_dropdown = hide_panel.payload_source_dropdown.clone();
    let payload_text_view = hide_panel.payload_text_view.clone();
    let continue_button = continue_button.clone();

    let update_state: Rc<dyn Fn()> = Rc::new(move || {
        let cover_ready = !cover_entry.text().trim().is_empty();
        let payload_ready = match selected_payload_mode(&payload_source_dropdown) {
            HidePayloadMode::File => !payload_file_entry.text().trim().is_empty(),
            HidePayloadMode::Text => text_payload_has_content(&payload_text_view),
        };
        continue_button.set_sensitive(cover_ready && payload_ready);
    });

    update_state();

    for entry in [
        hide_panel.cover_field.path_entry.clone(),
        hide_panel.payload_file_field.path_entry.clone(),
    ] {
        let update_state_clone = update_state.clone();
        entry.connect_changed(move |_| update_state_clone());
    }

    {
        let update_state_clone = update_state.clone();
        hide_panel
            .payload_source_dropdown
            .connect_selected_notify(move |_| update_state_clone());
    }

    hide_panel
        .payload_text_view
        .buffer()
        .connect_changed(move |_| update_state());
}

pub(super) fn connect_reset_button(hide_panel: &HidePanel) {
    let cover_entry = hide_panel.cover_field.path_entry.clone();
    let payload_file_entry = hide_panel.payload_file_field.path_entry.clone();
    let output_entry = hide_panel.output_field.path_entry.clone();
    let output_dir_entry = hide_panel.output_dir_field.path_entry.clone();
    let pattern_entry = hide_panel.output_pattern_entry.clone();
    let passphrase_file_entry = hide_panel.passphrase_file_field.path_entry.clone();
    let passphrase_entry = hide_panel.passphrase_text_entry.clone();
    let passphrase_confirm_entry = hide_panel.passphrase_confirm_entry.clone();
    let payload_text_view = hide_panel.payload_text_view.clone();

    hide_panel.reset_button.connect_clicked(move |_| {
        cover_entry.set_text("");
        payload_file_entry.set_text("");
        output_entry.set_text(&default_hide_output_path());
        output_dir_entry.set_text(&default_split_output_dir());
        pattern_entry.set_text("hidden_payload_part_{index}.png");
        passphrase_file_entry.set_text("");
        passphrase_entry.set_text("");
        passphrase_confirm_entry.set_text("");
        payload_text_view.buffer().set_text("");
    });
}

fn text_payload_has_content(payload_text_view: &gtk::TextView) -> bool {
    let payload_buffer = payload_text_view.buffer();
    // Avoid creating a full copy of the buffer text on every keystroke
    // This keeps step gating responsive even when large text is pasted
    if payload_buffer.char_count() == 0 {
        return false;
    }

    let mut cursor_iter = payload_buffer.start_iter();
    loop {
        let current_char = cursor_iter.char();
        if !current_char.is_whitespace() {
            return true;
        }

        // forward_char() returns false when the iterator is already at the end
        if !cursor_iter.forward_char() {
            break;
        }
    }

    false
}

fn output_pattern_has_index(pattern_text: &str) -> bool {
    let trimmed_pattern = pattern_text.trim();
    !trimmed_pattern.is_empty()
        && !trimmed_pattern.contains('/')
        && !trimmed_pattern.contains('\\')
        && (trimmed_pattern.contains("{i}")
            || trimmed_pattern.contains("{index}")
            || trimmed_pattern.contains("%04u")
            || trimmed_pattern.contains("%u"))
}
