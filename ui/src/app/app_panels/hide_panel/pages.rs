use gtk::prelude::*;
use gtk4 as gtk;

use super::cover::build_cover_section;
use super::state::wire_continue_to_preflight_state;
use super::stepper::go_to_step_button;
use crate::app::app_panels::output_plan::{OutputPlanView, wire_review_plan_button};
use crate::app::app_types::HidePanel;

pub(super) fn build_select_files_page(
    hide_panel: &HidePanel,
    workflow_stack: &gtk::Stack,
    _step_buttons: &[gtk::Button],
) -> gtk::Box {
    let page = build_workflow_page();
    let continue_button = go_to_step_button("Continue to Preflight", "preflight", workflow_stack);
    wire_continue_to_preflight_state(hide_panel, &continue_button);

    page.append(&build_cover_section(hide_panel));
    page.append(&build_payload_section(hide_panel));
    page.append(&build_navigation_row(None, Some(continue_button)));
    page
}

pub(super) fn build_preflight_page(
    hide_panel: &HidePanel,
    workflow_stack: &gtk::Stack,
    _step_buttons: &[gtk::Button],
    output_plan_view: &OutputPlanView,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) -> gtk::Box {
    let page = build_workflow_page();
    let review_button = gtk::Button::with_label("Review Plan");
    review_button.add_css_class("action");
    wire_review_plan_button(
        hide_panel,
        output_plan_view,
        cli_path_entry,
        status_label,
        log_buffer,
        workflow_stack,
        &review_button,
    );
    page.append(&build_preflight_options_section(hide_panel));
    page.append(&build_navigation_row(
        Some(go_to_step_button("Back", "files", workflow_stack)),
        Some(review_button),
    ));
    page
}

pub(super) fn build_review_page(
    output_plan_view: &OutputPlanView,
    workflow_stack: &gtk::Stack,
    _step_buttons: &[gtk::Button],
) -> gtk::Box {
    let page = build_workflow_page();
    page.append(&output_plan_view.section);
    page.append(&build_navigation_row(
        Some(go_to_step_button("Back", "preflight", workflow_stack)),
        Some(output_plan_view.cta_button.clone()),
    ));
    page
}

pub(super) fn build_hide_execute_page(
    hide_panel: &HidePanel,
    workflow_stack: &gtk::Stack,
    _step_buttons: &[gtk::Button],
) -> gtk::Box {
    let page = build_workflow_page();
    page.append(&build_passphrase_section(hide_panel));
    page.append(&build_hide_action_row(hide_panel, workflow_stack));
    page
}

fn build_workflow_page() -> gtk::Box {
    let page = gtk::Box::new(gtk::Orientation::Vertical, 14);
    page.add_css_class("workflow-page");
    page
}

fn build_navigation_row(
    back_button: Option<gtk::Button>,
    next_button: Option<gtk::Button>,
) -> gtk::Box {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    row.add_css_class("action-row");
    if let Some(button) = back_button {
        button.remove_css_class("action");
        button.add_css_class("secondary");
        row.append(&button);
    }
    let spacer = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    spacer.set_hexpand(true);
    row.append(&spacer);
    if let Some(button) = next_button {
        row.append(&button);
    }
    row
}

pub(super) fn numbered_title(number_text: &str, title_text: &str) -> gtk::Box {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    let title_label = gtk::Label::new(Some(&format!("{number_text}. {title_text}")));
    title_label.add_css_class("light-section-title");
    title_label.set_xalign(0.0);
    row.append(&title_label);
    row
}

fn build_payload_section(hide_panel: &HidePanel) -> gtk::Box {
    let section = gtk::Box::new(gtk::Orientation::Vertical, 10);
    section.add_css_class("light-section");
    section.append(&numbered_title("2", "Select a payload"));
    section.append(&build_segmented_selector(
        "Payload source",
        &hide_panel.payload_source_dropdown,
    ));
    section.append(&hide_panel.payload_stack);
    section
}

fn build_preflight_options_section(hide_panel: &HidePanel) -> gtk::Box {
    let section = gtk::Box::new(gtk::Orientation::Vertical, 10);
    section.add_css_class("light-section");
    section.append(&numbered_title("3", "Run preflight"));

    let option_grid = gtk::Grid::new();
    option_grid.add_css_class("option-grid");
    option_grid.set_column_spacing(10);
    option_grid.set_row_spacing(10);

    option_grid.attach(
        &build_labeled_dropdown("Method", &hide_panel.method_dropdown),
        0,
        0,
        1,
        1,
    );

    let help = gtk::Label::new(Some(
        "Preflight checks the real cover capacity and decides whether one image is enough",
    ));
    help.add_css_class("muted-label");
    help.set_xalign(0.0);
    help.set_wrap(true);
    help.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    section.append(&option_grid);
    section.append(&help);
    section
}

fn build_passphrase_section(hide_panel: &HidePanel) -> gtk::Box {
    let section = gtk::Box::new(gtk::Orientation::Vertical, 10);
    section.add_css_class("light-section");
    section.append(&numbered_title("4", "Hide payload"));

    let passphrase_stack_frame = gtk::Box::new(gtk::Orientation::Vertical, 8);
    passphrase_stack_frame.add_css_class("passphrase-frame");
    passphrase_stack_frame.append(&build_segmented_selector(
        "Passphrase",
        &hide_panel.passphrase_source_dropdown,
    ));
    passphrase_stack_frame.append(&hide_panel.passphrase_stack);

    section.append(&passphrase_stack_frame);
    section
}

fn build_segmented_selector(label_text: &str, dropdown: &gtk::DropDown) -> gtk::Box {
    let box_widget = build_labeled_dropdown(label_text, dropdown);
    box_widget.add_css_class("segmented-field");
    box_widget
}

fn build_labeled_dropdown(label_text: &str, dropdown: &gtk::DropDown) -> gtk::Box {
    let box_widget = gtk::Box::new(gtk::Orientation::Vertical, 6);
    box_widget.add_css_class("option-field");

    let label = gtk::Label::new(Some(label_text));
    label.add_css_class("compact-field-label");
    label.set_xalign(0.0);

    box_widget.append(&label);
    box_widget.append(dropdown);
    box_widget
}

fn build_hide_action_row(hide_panel: &HidePanel, workflow_stack: &gtk::Stack) -> gtk::Box {
    let row = gtk::Box::new(gtk::Orientation::Horizontal, 12);
    row.add_css_class("action-row");
    row.add_css_class("final-action-row");

    let back_button = go_to_step_button("Back", "review", workflow_stack);
    back_button.remove_css_class("action");
    back_button.add_css_class("secondary");

    let spacer = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    spacer.set_hexpand(true);

    row.append(&back_button);
    row.append(&spacer);
    row.append(&hide_panel.reset_button);
    row.append(&hide_panel.run_button);
    row
}
