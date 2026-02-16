use std::rc::Rc;

use gtk::prelude::*;
use gtk4 as gtk;

use super::{
    build_feature_badge_row, build_hide_passphrase_stack, build_passphrase_text_entry,
    build_payload_stack, build_payload_text_view, build_step_heading, build_step_revealer,
};
use crate::app::app_fields::{
    build_file_field_row, build_method_dropdown, build_method_row,
    build_passphrase_source_dropdown, build_passphrase_source_row, build_payload_source_dropdown,
    build_payload_source_row, selected_passphrase_mode, selected_payload_mode,
};
use crate::app::app_types::{HidePanel, HidePayloadMode, PassphraseMode};
use crate::command_builder::default_hide_output_path;

// Build all hide controls and pre-wire source switching
pub fn build_hide_panel(
    window: &gtk::ApplicationWindow,
    log_buffer: &gtk::TextBuffer,
) -> HidePanel {
    let cover_field = build_file_field_row(
        window,
        log_buffer,
        "Cover Image",
        "select .png / .jxl / .bmp / .ppm / .jpg / .jpeg / .webp cover file",
        gtk::FileChooserAction::Open,
    );
    let payload_file_field = build_file_field_row(
        window,
        log_buffer,
        "Payload File",
        "select payload file to hide",
        gtk::FileChooserAction::Open,
    );
    let output_field = build_file_field_row(
        window,
        log_buffer,
        "Output Image",
        "defaulted automatically, edit if needed",
        gtk::FileChooserAction::Save,
    );
    output_field
        .path_entry
        .set_text(&default_hide_output_path());
    let passphrase_file_field = build_file_field_row(
        window,
        log_buffer,
        "Passphrase File",
        "select text file containing passphrase",
        gtk::FileChooserAction::Open,
    );

    let payload_source_dropdown = build_payload_source_dropdown();
    let payload_text_view = build_payload_text_view();
    let payload_stack = build_payload_stack(&payload_file_field.container_box, &payload_text_view);

    let passphrase_source_dropdown = build_passphrase_source_dropdown();
    let passphrase_text_entry = build_passphrase_text_entry();
    let passphrase_confirm_entry = build_passphrase_text_entry();
    let passphrase_stack = build_hide_passphrase_stack(
        &passphrase_file_field.container_box,
        &passphrase_text_entry,
        &passphrase_confirm_entry,
    );

    // Keep payload stack in sync with selected source
    let payload_stack_clone = payload_stack.clone();
    payload_source_dropdown.connect_selected_notify(move |dropdown| match dropdown.selected() {
        1 => payload_stack_clone.set_visible_child_name("text"),
        _ => payload_stack_clone.set_visible_child_name("file"),
    });

    // Keep passphrase stack in sync with selected source
    let passphrase_stack_clone = passphrase_stack.clone();
    passphrase_source_dropdown.connect_selected_notify(move |dropdown| match dropdown.selected() {
        1 => passphrase_stack_clone.set_visible_child_name("file"),
        _ => passphrase_stack_clone.set_visible_child_name("text"),
    });

    let method_dropdown = build_method_dropdown();

    let run_button = gtk::Button::with_label("Encrypt + Hide Payload");
    run_button.add_css_class("action");
    run_button.add_css_class("primary-cta");
    let run_button_clone = run_button.clone();
    payload_source_dropdown.connect_selected_notify(move |dropdown| match dropdown.selected() {
        1 => run_button_clone.set_label("Encrypt + Hide Text"),
        _ => run_button_clone.set_label("Encrypt + Hide Payload"),
    });

    HidePanel {
        cover_field,
        payload_file_field,
        output_field,
        passphrase_file_field,
        payload_source_dropdown,
        payload_stack,
        payload_text_view,
        passphrase_source_dropdown,
        passphrase_stack,
        passphrase_text_entry,
        passphrase_confirm_entry,
        method_dropdown,
        run_button,
    }
}

// Assemble hide card as a step-by-step flow to reduce cognitive load
pub fn assemble_hide_card(hide_panel: &HidePanel) -> gtk::Box {
    let panel_box = gtk::Box::new(gtk::Orientation::Vertical, 12);
    panel_box.add_css_class("workflow-panel");

    let title = gtk::Label::new(Some("Hide Payload into Cover Image"));
    title.add_css_class("section-title");
    title.set_xalign(0.0);

    let caption = gtk::Label::new(Some(
        "Follow each step in order, fields unlock as required inputs are provided",
    ));
    caption.add_css_class("section-caption");
    caption.set_xalign(0.0);

    // Summary badges keep key guarantees visible even before form interaction
    let badge_row = build_feature_badge_row(&[
        ("encrypted", "badge-success"),
        ("authenticated", "badge-info"),
        ("lossless output", "badge-warning"),
    ]);

    let step_one_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_one_box.add_css_class("step-card");
    step_one_box.append(&build_step_heading(
        "Step 1",
        "Choose a cover image",
        "Input supports .png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp; output stays lossless",
    ));
    step_one_box.append(&hide_panel.cover_field.container_box);

    let step_two_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_two_box.add_css_class("step-card");
    step_two_box.append(&build_step_heading(
        "Step 2",
        "Provide payload",
        "File mode reads from disk, pasted text mode keeps payload in memory",
    ));
    step_two_box.append(&build_payload_source_row(
        &hide_panel.payload_source_dropdown,
    ));
    step_two_box.append(&hide_panel.payload_stack);

    let step_three_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_three_box.add_css_class("step-card");
    step_three_box.append(&build_step_heading(
        "Step 3",
        "Choose output image path",
        "A safe default is prefilled and can be changed",
    ));
    step_three_box.append(&hide_panel.output_field.container_box);

    let step_four_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_four_box.add_css_class("step-card");
    step_four_box.append(&build_step_heading(
        "Step 4",
        "Security settings",
        "Use typed passphrase by default, file mode is available for automation",
    ));
    step_four_box.append(&build_passphrase_source_row(
        &hide_panel.passphrase_source_dropdown,
    ));
    step_four_box.append(&hide_panel.passphrase_stack);
    step_four_box.append(&build_method_row(&hide_panel.method_dropdown));

    let step_five_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    step_five_box.add_css_class("step-card");
    step_five_box.append(&build_step_heading(
        "Step 5",
        "Run hide operation",
        "Validation runs before command execution",
    ));
    step_five_box.append(&hide_panel.run_button);

    let step_two_revealer = build_step_revealer(&step_two_box);
    let step_three_revealer = build_step_revealer(&step_three_box);
    let step_four_revealer = build_step_revealer(&step_four_box);
    let step_five_revealer = build_step_revealer(&step_five_box);

    panel_box.append(&title);
    panel_box.append(&caption);
    panel_box.append(&badge_row);
    panel_box.append(&step_one_box);
    panel_box.append(&step_two_revealer);
    panel_box.append(&step_three_revealer);
    panel_box.append(&step_four_revealer);
    panel_box.append(&step_five_revealer);

    connect_hide_step_visibility(
        hide_panel,
        &step_two_revealer,
        &step_three_revealer,
        &step_four_revealer,
        &step_five_revealer,
    );

    panel_box
}

fn connect_hide_step_visibility(
    hide_panel: &HidePanel,
    step_two_revealer: &gtk::Revealer,
    step_three_revealer: &gtk::Revealer,
    step_four_revealer: &gtk::Revealer,
    step_five_revealer: &gtk::Revealer,
) {
    let cover_entry = hide_panel.cover_field.path_entry.clone();
    let payload_file_entry = hide_panel.payload_file_field.path_entry.clone();
    let output_entry = hide_panel.output_field.path_entry.clone();
    let passphrase_file_entry = hide_panel.passphrase_file_field.path_entry.clone();
    let passphrase_text_entry = hide_panel.passphrase_text_entry.clone();
    let passphrase_confirm_entry = hide_panel.passphrase_confirm_entry.clone();
    let payload_source_dropdown = hide_panel.payload_source_dropdown.clone();
    let payload_text_view = hide_panel.payload_text_view.clone();
    let passphrase_source_dropdown = hide_panel.passphrase_source_dropdown.clone();

    let step_two_revealer_clone = step_two_revealer.clone();
    let step_three_revealer_clone = step_three_revealer.clone();
    let step_four_revealer_clone = step_four_revealer.clone();
    let step_five_revealer_clone = step_five_revealer.clone();

    let update_step_visibility: Rc<dyn Fn()> = Rc::new(move || {
        let cover_ready = !cover_entry.text().trim().is_empty();

        let payload_ready = match selected_payload_mode(&payload_source_dropdown) {
            HidePayloadMode::File => !payload_file_entry.text().trim().is_empty(),
            HidePayloadMode::Text => text_payload_has_content(&payload_text_view),
        };

        let output_ready = !output_entry.text().trim().is_empty();

        let passphrase_ready = match selected_passphrase_mode(&passphrase_source_dropdown) {
            PassphraseMode::File => !passphrase_file_entry.text().trim().is_empty(),
            PassphraseMode::Text => {
                let passphrase_text = passphrase_text_entry.text();
                let confirm_text = passphrase_confirm_entry.text();
                !passphrase_text.trim().is_empty()
                    && passphrase_text.as_str() == confirm_text.as_str()
            }
        };

        step_two_revealer_clone.set_reveal_child(cover_ready);
        step_three_revealer_clone.set_reveal_child(cover_ready && payload_ready);
        step_four_revealer_clone.set_reveal_child(cover_ready && payload_ready && output_ready);
        step_five_revealer_clone
            .set_reveal_child(cover_ready && payload_ready && output_ready && passphrase_ready);
    });

    // Evaluate once so the UI reflects any prefilled defaults immediately
    update_step_visibility();

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel.cover_field.path_entry.connect_changed(move |_| {
            update_step_visibility_clone();
        });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel
            .payload_file_field
            .path_entry
            .connect_changed(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel
            .output_field
            .path_entry
            .connect_changed(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel
            .passphrase_file_field
            .path_entry
            .connect_changed(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel.passphrase_text_entry.connect_changed(move |_| {
            update_step_visibility_clone();
        });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel
            .passphrase_confirm_entry
            .connect_changed(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel
            .payload_source_dropdown
            .connect_selected_notify(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility.clone();
        hide_panel
            .passphrase_source_dropdown
            .connect_selected_notify(move |_| {
                update_step_visibility_clone();
            });
    }

    {
        let update_step_visibility_clone = update_step_visibility;
        hide_panel
            .payload_text_view
            .buffer()
            .connect_changed(move |_| {
                update_step_visibility_clone();
            });
    }
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
