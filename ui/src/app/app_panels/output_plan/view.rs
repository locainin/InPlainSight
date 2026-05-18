use gtk::prelude::*;
use gtk4 as gtk;

use super::hero::{
    build_output_plan_hero, build_single_output_plan_icon, build_split_output_plan_icon,
};
use super::shared::build_change_cover_callout;
use super::single::{build_single_details_column, build_single_result_column};
use super::split::{build_split_output_destination_card, build_split_output_plan_result_card};
use super::types::{OutputPlanView, SingleOutputPlanWidgets, SplitOutputPlanWidgets};
use crate::app::app_types::HidePanel;

// Build the step-three output plan view with single and split branches
pub fn build_output_plan_view(
    hide_panel: &HidePanel,
    workflow_stack: &gtk::Stack,
) -> OutputPlanView {
    // The mode stack is switched only after preflight reports the real plan
    let section = gtk::Box::new(gtk::Orientation::Vertical, 0);
    section.add_css_class("light-section");
    section.add_css_class("output-plan-section");

    let mode_stack = gtk::Stack::builder()
        .transition_type(gtk::StackTransitionType::Crossfade)
        .transition_duration(140)
        .hexpand(true)
        .vexpand(true)
        .build();
    let (split_section, split_widgets) = build_split_output_plan_section(hide_panel);
    let (single_section, single_widgets) = build_single_output_plan_section(hide_panel);
    // Split is the initial child because it contains pending-safe copy and no false success
    mode_stack.add_named(&split_section, Some("split"));
    mode_stack.add_named(&single_section, Some("single"));
    mode_stack.set_visible_child_name("split");

    let cta_button = gtk::Button::with_label("Create Images");
    cta_button.add_css_class("action");
    cta_button.add_css_class("primary-cta");
    let workflow_stack_clone = workflow_stack.clone();
    cta_button.connect_clicked(move |_| {
        // Confirmation and passphrase entry happen on the final hide step
        workflow_stack_clone.set_visible_child_name("hide");
    });

    section.append(&mode_stack);

    OutputPlanView {
        section,
        mode_stack,
        split: split_widgets,
        single: single_widgets,
        cta_button,
    }
}

fn build_split_output_plan_section(hide_panel: &HidePanel) -> (gtk::Box, SplitOutputPlanWidgets) {
    // Split layout matches the concept: result card left, output location right
    let section = gtk::Box::new(gtk::Orientation::Vertical, 18);
    let output_grid = gtk::Box::new(gtk::Orientation::Horizontal, 20);
    output_grid.add_css_class("output-plan-grid");
    output_grid.set_hexpand(true);

    let (result_card, mut split_widgets) = build_split_output_plan_result_card();
    let (destination_card, file_count_label, file_name_labels, file_size_labels, file_rows) =
        build_split_output_destination_card(hide_panel);
    // Store row handles so preflight can fill names, sizes, and visibility later
    split_widgets.file_count_label = file_count_label;
    split_widgets.file_name_labels = file_name_labels;
    split_widgets.file_size_labels = file_size_labels;
    split_widgets.file_rows = file_rows;
    output_grid.append(&result_card);
    output_grid.append(&destination_card);

    let plan_icon = build_split_output_plan_icon();
    section.append(&build_output_plan_hero(
        &plan_icon,
        "This payload requires multiple images",
        "The selected cover image doesn't have enough capacity for this payload, so it will be split across multiple lossless images.",
    ));
    section.append(&output_grid);
    section.append(&build_change_cover_callout());
    (section, split_widgets)
}

fn build_single_output_plan_section(hide_panel: &HidePanel) -> (gtk::Box, SingleOutputPlanWidgets) {
    // Single layout keeps output path controls on the left and final image details on the right
    let section = gtk::Box::new(gtk::Orientation::Vertical, 18);
    let output_grid = gtk::Box::new(gtk::Orientation::Horizontal, 20);
    output_grid.add_css_class("output-plan-grid");
    output_grid.set_hexpand(true);

    let (result_column, single_widgets) = build_single_result_column(hide_panel);
    output_grid.append(&result_column);
    output_grid.append(&build_single_details_column(hide_panel, &single_widgets));

    let plan_icon = build_single_output_plan_icon();
    section.append(&build_output_plan_hero(
        &plan_icon,
        "This payload fits in one image",
        "After compression, the payload is smaller than the available capacity of the selected cover image. It will be embedded into a single lossless image.",
    ));
    section.append(&output_grid);
    (section, single_widgets)
}
