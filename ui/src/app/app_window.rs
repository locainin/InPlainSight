use gtk::prelude::*;
use gtk4 as gtk;

use super::app_chrome::{
    build_footer, build_sidebar, build_sidebar_button, build_titlebar, wire_sidebar_navigation,
};
use super::app_execution::{wire_extract_execution, wire_hide_execution};
use super::app_logging::wire_clear_log_button;
use super::app_panels::{
    assemble_extract_card, assemble_hide_card, build_extract_panel, build_hide_panel,
    build_plan_summary_panel,
};
use crate::command_builder::default_cli_binary_path;

// Build and wire the full GTK application window
pub fn build_ui(application: &gtk::Application) {
    // Window sizing targets a desktop workflow while remaining scrollable on smaller screens
    let window = gtk::ApplicationWindow::builder()
        .application(application)
        .title("InPlainSight")
        .default_width(1180)
        .default_height(720)
        .build();

    // Shared state that crosses panels lives at the window level
    let log_buffer = gtk::TextBuffer::new(None);
    let hide_panel = build_hide_panel(&window, &log_buffer);
    let extract_panel = build_extract_panel(&window, &log_buffer);
    let cli_path_entry = build_hidden_cli_path_entry();
    let status_label = build_status_label();
    let theme_button = build_theme_button();

    // Navigation, side summary, and execution hooks are wired after all widgets exist
    let workflow_stack = build_workflow_stack(
        &hide_panel,
        &extract_panel,
        &cli_path_entry,
        &status_label,
        &log_buffer,
    );
    let hide_nav_button = build_sidebar_button("Hide", true);
    let extract_nav_button = build_sidebar_button("Extract", false);
    wire_sidebar_navigation(
        &workflow_stack,
        &hide_nav_button,
        &extract_nav_button,
        &hide_panel.run_button,
        &extract_panel.run_button,
    );

    let sidebar = build_sidebar(&hide_nav_button, &extract_nav_button, &theme_button);
    let plan_summary = build_plan_summary_panel(&hide_panel, &extract_panel, &log_buffer);
    let root = build_root_shell(
        &workflow_stack,
        &plan_summary.container,
        &sidebar.container,
        &status_label,
    );

    wire_theme_button(&root, &theme_button);
    wire_clear_log_button(&plan_summary.clear_log_button, &log_buffer, &status_label);
    wire_hide_execution(
        &hide_panel,
        window.upcast_ref(),
        &cli_path_entry,
        &status_label,
        &log_buffer,
    );
    wire_extract_execution(&extract_panel, &cli_path_entry, &status_label, &log_buffer);

    // The hidden CLI path keeps command construction configurable without visible clutter
    root.append(&cli_path_entry);
    window.set_child(Some(&root));
    window.present();
}

fn build_hidden_cli_path_entry() -> gtk::Entry {
    // The path entry is invisible but remains a normal GTK value source for command wiring
    let cli_path_entry = gtk::Entry::new();
    cli_path_entry.add_css_class("entry");
    cli_path_entry.set_text(&default_cli_binary_path());
    cli_path_entry.set_visible(false);
    cli_path_entry
}

fn build_status_label() -> gtk::Label {
    // The status label is intentionally width-limited to avoid titlebar/sidebar shifts
    let status_label = gtk::Label::new(None);
    status_label.set_markup("<span foreground='#10b981'>●</span>  Ready");
    status_label.add_css_class("sidebar-status");
    status_label.set_xalign(0.0);
    status_label.set_width_chars(18);
    status_label.set_max_width_chars(18);
    status_label.set_ellipsize(gtk::pango::EllipsizeMode::End);
    status_label
}

fn build_theme_button() -> gtk::Button {
    // Theme toggle lives in the sidebar so the titlebar stays visually stable
    let theme_button = gtk::Button::with_label("☀  Light");
    theme_button.add_css_class("sidebar-button");
    theme_button.add_css_class("sidebar-tool-link");
    theme_button.set_tooltip_text(Some("Switch to light mode"));
    theme_button
}

fn build_workflow_stack(
    hide_panel: &super::app_types::HidePanel,
    extract_panel: &super::app_types::ExtractPanel,
    cli_path_entry: &gtk::Entry,
    status_label: &gtk::Label,
    log_buffer: &gtk::TextBuffer,
) -> gtk::Stack {
    // The top-level stack switches product mode, while hide has its own step stack
    let hide_content = assemble_hide_card(hide_panel, cli_path_entry, status_label, log_buffer);
    let extract_content = assemble_extract_card(extract_panel);
    let workflow_stack = gtk::Stack::builder()
        .transition_type(gtk::StackTransitionType::Crossfade)
        .transition_duration(160)
        .hexpand(true)
        .vexpand(true)
        .build();
    workflow_stack.add_named(&hide_content, Some("hide"));
    workflow_stack.add_named(&extract_content, Some("extract"));
    workflow_stack.set_visible_child_name("hide");
    workflow_stack
}

fn build_root_shell(
    workflow_stack: &gtk::Stack,
    plan_summary: &gtk::Box,
    sidebar: &gtk::Box,
    status_label: &gtk::Label,
) -> gtk::Box {
    // Workflow and inspector scroll independently so logs never resize the main page
    let workflow_scroll = gtk::ScrolledWindow::new();
    workflow_scroll.add_css_class("light-workflow-scroll");
    workflow_scroll.set_hscrollbar_policy(gtk::PolicyType::Never);
    workflow_scroll.set_vexpand(true);
    workflow_scroll.set_hexpand(true);
    workflow_scroll.set_child(Some(workflow_stack));

    // The inspector has a fixed width, which prevents dark/light mode layout drift
    let inspector_scroll = gtk::ScrolledWindow::new();
    inspector_scroll.add_css_class("inspector-scroll");
    inspector_scroll.set_hscrollbar_policy(gtk::PolicyType::Never);
    inspector_scroll.set_vscrollbar_policy(gtk::PolicyType::Automatic);
    inspector_scroll.set_vexpand(true);
    inspector_scroll.set_width_request(430);
    inspector_scroll.set_child(Some(plan_summary));

    let main_body = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    main_body.add_css_class("main-body");
    main_body.set_hexpand(true);
    main_body.set_vexpand(true);
    main_body.append(&workflow_scroll);
    main_body.append(&inspector_scroll);

    let content = gtk::Box::new(gtk::Orientation::Vertical, 0);
    content.add_css_class("content-shell");
    content.set_hexpand(true);
    content.set_vexpand(true);
    content.append(&main_body);

    let body = gtk::Box::new(gtk::Orientation::Horizontal, 0);
    body.add_css_class("app-body");
    body.set_hexpand(true);
    body.set_vexpand(true);
    body.append(sidebar);
    body.append(&content);

    // The titlebar is part of the app root for custom macOS-style window chrome
    let root = gtk::Box::new(gtk::Orientation::Vertical, 0);
    root.add_css_class("app-root");
    root.add_css_class("dark-mode");
    root.set_hexpand(true);
    root.set_vexpand(true);
    root.append(&build_titlebar(status_label));
    root.append(&body);
    root.append(&build_footer());
    root
}

fn wire_theme_button(root: &gtk::Box, theme_button: &gtk::Button) {
    // Theme state is local because persisted preferences are not part of this UI flow
    let light_mode_enabled = std::rc::Rc::new(std::cell::Cell::new(false));
    let root_clone = root.clone();
    let theme_button_clone = theme_button.clone();
    theme_button.connect_clicked(move |_| {
        let next_light_mode = !light_mode_enabled.get();
        light_mode_enabled.set(next_light_mode);
        if next_light_mode {
            // Swap only theme classes so widget geometry stays unchanged
            root_clone.remove_css_class("dark-mode");
            root_clone.add_css_class("light-mode");
            theme_button_clone.set_label("☾  Dark");
            theme_button_clone.set_tooltip_text(Some("Switch to dark mode"));
        } else {
            // Dark mode is the default visual design and uses the same layout metrics
            root_clone.remove_css_class("light-mode");
            root_clone.add_css_class("dark-mode");
            theme_button_clone.set_label("☀  Light");
            theme_button_clone.set_tooltip_text(Some("Switch to light mode"));
        }
    });
}
