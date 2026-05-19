use gtk::prelude::*;
use gtk4 as gtk;

use super::pages::{
    build_hide_execute_page, build_preflight_page, build_review_page, build_select_files_page,
};
use super::state::{connect_hide_step_visibility, connect_reset_button, connect_run_button_copy};
use super::stepper::build_stepper;
use crate::app::app_panels::output_plan::build_output_plan_view;
use crate::app::app_types::HidePanel;

// Assemble the hide workflow shell and its four step pages
pub fn assemble_hide_card(
    hide_panel: &HidePanel,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) -> gtk::Box {
    // This file owns composition only; individual pages own their own widgets
    let panel_box = gtk::Box::new(gtk::Orientation::Vertical, 14);
    panel_box.add_css_class("workflow-panel");

    let workflow_stack = gtk::Stack::builder()
        .transition_type(gtk::StackTransitionType::SlideLeftRight)
        .transition_duration(180)
        .hexpand(true)
        .vexpand(true)
        .build();
    let (stepper, step_buttons) = build_stepper(&workflow_stack);
    // Step three is built once and updated after CLI preflight completes
    let output_plan_view = build_output_plan_view(hide_panel, &workflow_stack);

    workflow_stack.add_named(
        &build_select_files_page(hide_panel, &workflow_stack, &step_buttons),
        Some("files"),
    );
    workflow_stack.add_named(
        &build_preflight_page(
            hide_panel,
            &workflow_stack,
            &step_buttons,
            &output_plan_view,
            cli_path_entry,
            status_label,
            log_buffer,
        ),
        Some("preflight"),
    );
    workflow_stack.add_named(
        &build_review_page(&output_plan_view, &workflow_stack, &step_buttons),
        Some("review"),
    );
    workflow_stack.add_named(
        &build_hide_execute_page(hide_panel, &workflow_stack, &step_buttons),
        Some("hide"),
    );
    workflow_stack.set_visible_child_name("files");
    {
        // Force the initial page after GTK has realized the stack
        // Some backends briefly report the first added child during startup
        let workflow_stack = workflow_stack.clone();
        gtk::glib::idle_add_local_once(move || workflow_stack.set_visible_child_name("files"));
    }

    panel_box.append(&stepper);
    panel_box.append(&workflow_stack);
    // Field-level signals are connected after every page has been attached
    connect_hide_step_visibility(hide_panel);
    connect_reset_button(hide_panel);
    connect_run_button_copy(hide_panel);

    panel_box
}
