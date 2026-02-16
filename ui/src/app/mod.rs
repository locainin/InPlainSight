use gtk::prelude::*;
use gtk4 as gtk;

mod app_execution;
mod app_fields;
mod app_logging;
mod app_panels;
mod app_types;

use app_execution::{wire_extract_execution, wire_hide_execution};
use app_logging::wire_clear_log_button;
use app_panels::{
    assemble_extract_card, assemble_hide_card, build_extract_panel, build_hide_panel,
};

use crate::command_builder::default_cli_binary_path;

// Build a compact header badge used for workflow guarantees and hints
fn build_header_badge(badge_text: &str, css_class_name: &str) -> gtk::Label {
    let badge_label = gtk::Label::new(Some(badge_text));
    badge_label.add_css_class("hero-badge");
    badge_label.add_css_class(css_class_name);
    badge_label.set_xalign(0.0);
    badge_label
}

// Count visible log lines so the UI can show how much history is currently loaded
fn count_log_lines(log_buffer: &gtk::TextBuffer) -> usize {
    let start_iter = log_buffer.start_iter();
    let end_iter = log_buffer.end_iter();
    let full_log_text = log_buffer.text(&start_iter, &end_iter, false);

    full_log_text
        .lines()
        .filter(|line_text| !line_text.trim().is_empty())
        .count()
}

// Build the full GTK window and wire actions between panels and command execution
pub fn build_ui(application: &gtk::Application) {
    let window = gtk::ApplicationWindow::builder()
        .application(application)
        .title("InPlainSight Studio")
        .default_width(1240)
        .default_height(820)
        .build();
    window.set_size_request(960, 640);

    let root_box = gtk::Box::new(gtk::Orientation::Vertical, 14);
    root_box.add_css_class("app-root");
    root_box.set_margin_top(18);
    root_box.set_margin_bottom(18);
    root_box.set_margin_start(18);
    root_box.set_margin_end(18);
    root_box.set_vexpand(true);

    let title_label = gtk::Label::new(Some("InPlainSight Studio"));
    title_label.set_widget_name("main-title");
    title_label.set_xalign(0.0);

    let subtitle_label = gtk::Label::new(Some(
        "Encrypt, hide, and recover files through a guided, production-style desktop workflow",
    ));
    subtitle_label.set_widget_name("main-subtitle");
    subtitle_label.set_xalign(0.0);
    // Keep the header readable when the window is narrow
    subtitle_label.set_wrap(true);
    subtitle_label.set_wrap_mode(gtk::pango::WrapMode::WordChar);

    let status_prefix_label = gtk::Label::new(Some("Status"));
    status_prefix_label.add_css_class("section-caption");
    status_prefix_label.set_xalign(0.0);

    let status_label = gtk::Label::new(Some("Ready"));
    status_label.set_widget_name("status-label");
    status_label.set_xalign(0.0);
    status_label.add_css_class("status-ready");

    let status_row = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    status_row.add_css_class("status-row");
    status_row.append(&status_prefix_label);
    status_row.append(&status_label);

    let title_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    title_box.append(&title_label);
    title_box.append(&subtitle_label);
    title_box.append(&status_row);

    let hero_badge_row = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    hero_badge_row.add_css_class("hero-badge-row");
    hero_badge_row.append(&build_header_badge("xchacha20-poly1305", "hero-badge-key"));
    hero_badge_row.append(&build_header_badge("argon2id KDF", "hero-badge-key"));
    hero_badge_row.append(&build_header_badge(
        "lossless output enforced",
        "hero-badge-safe",
    ));

    let cli_path_entry = gtk::Entry::new();
    cli_path_entry.add_css_class("entry");
    cli_path_entry.add_css_class("cli-path-entry");
    cli_path_entry.set_hexpand(true);
    cli_path_entry.set_text(&default_cli_binary_path());
    cli_path_entry.set_placeholder_text(Some("path to inplainsight CLI binary"));

    let cli_path_label = gtk::Label::new(Some("CLI Binary"));
    cli_path_label.add_css_class("section-caption");
    cli_path_label.set_xalign(0.0);

    // Advanced block keeps beginner flow clean while still exposing power-user controls
    let cli_path_box = gtk::Box::new(gtk::Orientation::Vertical, 6);
    cli_path_box.add_css_class("advanced-card");
    cli_path_box.append(&cli_path_label);
    cli_path_box.append(&cli_path_entry);

    let advanced_expander = gtk::Expander::builder().label("Advanced Settings").build();
    advanced_expander.add_css_class("advanced-expander");
    advanced_expander.set_child(Some(&cli_path_box));
    advanced_expander.set_expanded(false);

    let hero_card = gtk::Box::new(gtk::Orientation::Vertical, 10);
    hero_card.add_css_class("card");
    hero_card.add_css_class("hero-card");
    hero_card.append(&title_box);
    hero_card.append(&hero_badge_row);
    hero_card.append(&advanced_expander);

    // Log buffer is created before panels so file pickers can log selection errors
    let log_buffer = gtk::TextBuffer::new(None);

    let hide_panel = build_hide_panel(&window, &log_buffer);
    let extract_panel = build_extract_panel(&window, &log_buffer);

    // Main mode stack for hide and extract views
    let workflow_stack = gtk::Stack::builder()
        .transition_type(gtk::StackTransitionType::SlideLeftRight)
        .transition_duration(220)
        .hexpand(true)
        .vexpand(true)
        .build();

    workflow_stack.add_titled(
        &assemble_hide_card(&hide_panel),
        Some("hide"),
        "Hide Payload",
    );
    workflow_stack.add_titled(
        &assemble_extract_card(&extract_panel),
        Some("extract"),
        "Extract Payload",
    );

    let workflow_switcher = gtk::StackSwitcher::builder().stack(&workflow_stack).build();
    workflow_switcher.add_css_class("workflow-switcher");

    let workflow_hint = gtk::Label::new(Some(
        "Required fields unlock the next step automatically; run buttons activate only when forms are complete",
    ));
    workflow_hint.add_css_class("section-caption");
    workflow_hint.add_css_class("workflow-hint");
    workflow_hint.set_xalign(0.0);

    let workflow_header = gtk::Box::new(gtk::Orientation::Vertical, 8);
    workflow_header.append(&workflow_switcher);
    workflow_header.append(&workflow_hint);

    let workflow_card = gtk::Box::new(gtk::Orientation::Vertical, 10);
    workflow_card.add_css_class("card");
    workflow_card.add_css_class("workflow-card");
    workflow_card.set_hexpand(true);
    workflow_card.set_vexpand(true);
    workflow_card.append(&workflow_header);
    workflow_card.append(&workflow_stack);

    let workflow_scrolled_window = gtk::ScrolledWindow::new();
    workflow_scrolled_window.add_css_class("workflow-scroll");
    workflow_scrolled_window.set_vexpand(true);
    workflow_scrolled_window.set_hexpand(true);
    workflow_scrolled_window.set_hscrollbar_policy(gtk::PolicyType::Never);
    workflow_scrolled_window.set_child(Some(&workflow_card));

    let log_text_view = gtk::TextView::new();
    log_text_view.set_editable(false);
    log_text_view.set_cursor_visible(false);
    log_text_view.add_css_class("entry");
    log_text_view.add_css_class("log-view");
    log_text_view.set_monospace(true);
    log_text_view.set_vexpand(true);

    log_text_view.set_buffer(Some(&log_buffer));

    let log_scrolled_window = gtk::ScrolledWindow::new();
    log_scrolled_window.add_css_class("log-scroll");
    log_scrolled_window.set_min_content_height(240);
    log_scrolled_window.set_vexpand(true);
    log_scrolled_window.set_child(Some(&log_text_view));

    let log_title = gtk::Label::new(Some("Execution Log"));
    log_title.add_css_class("section-title");
    log_title.set_xalign(0.0);

    let log_caption = gtk::Label::new(Some(
        "Structured validation, command output, and troubleshooting hints are streamed here",
    ));
    log_caption.add_css_class("section-caption");
    log_caption.set_xalign(0.0);

    let log_line_count_label = gtk::Label::new(Some("0 lines"));
    log_line_count_label.add_css_class("log-count-label");
    log_line_count_label.set_hexpand(true);
    log_line_count_label.set_halign(gtk::Align::End);
    log_line_count_label.set_xalign(1.0);

    let hide_log_button = gtk::Button::with_label("Hide");
    hide_log_button.add_css_class("secondary");
    hide_log_button.add_css_class("log-hide-button");

    let log_header_row = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    log_header_row.append(&log_title);
    log_header_row.append(&log_line_count_label);
    log_header_row.append(&hide_log_button);

    let clear_logs_button = gtk::Button::with_label("Clear Log");
    clear_logs_button.add_css_class("secondary");

    let log_controls_row = gtk::Box::new(gtk::Orientation::Horizontal, 8);
    log_controls_row.add_css_class("log-controls-row");
    log_controls_row.append(&clear_logs_button);

    let log_card = gtk::Box::new(gtk::Orientation::Vertical, 10);
    log_card.add_css_class("card");
    log_card.add_css_class("log-card");
    log_card.set_size_request(320, -1);
    log_card.append(&log_header_row);
    log_card.append(&log_caption);
    log_card.append(&log_scrolled_window);
    log_card.append(&log_controls_row);

    let content_paned = gtk::Paned::new(gtk::Orientation::Horizontal);
    content_paned.add_css_class("content-paned");
    content_paned.set_start_child(Some(&workflow_scrolled_window));
    content_paned.set_end_child(Some(&log_card));
    content_paned.set_resize_start_child(true);
    content_paned.set_resize_end_child(false);
    content_paned.set_shrink_start_child(true);
    content_paned.set_shrink_end_child(true);
    content_paned.set_wide_handle(true);
    content_paned.set_position(840);
    content_paned.set_vexpand(true);

    // Small right-edge handle restores hidden log card
    let show_log_handle_button = gtk::Button::with_label("Logs");
    show_log_handle_button.add_css_class("secondary");
    show_log_handle_button.add_css_class("log-restore-handle");
    show_log_handle_button.set_halign(gtk::Align::End);
    show_log_handle_button.set_valign(gtk::Align::Center);
    show_log_handle_button.set_margin_end(6);
    show_log_handle_button.set_visible(false);

    let content_overlay = gtk::Overlay::new();
    content_overlay.set_child(Some(&content_paned));
    content_overlay.add_overlay(&show_log_handle_button);
    content_overlay.set_vexpand(true);

    // Hide and restore callbacks keep controls intuitive on small windows
    {
        let log_card_clone = log_card.clone();
        let show_handle_clone = show_log_handle_button.clone();
        hide_log_button.connect_clicked(move |_| {
            log_card_clone.set_visible(false);
            show_handle_clone.set_visible(true);
        });
    }

    {
        let log_card_clone = log_card.clone();
        let show_handle_clone = show_log_handle_button.clone();
        show_log_handle_button.connect_clicked(move |_| {
            log_card_clone.set_visible(true);
            show_handle_clone.set_visible(false);
        });
    }

    // Log line counter updates after each append/clear operation
    let log_buffer_clone = log_buffer.clone();
    let log_line_count_label_clone = log_line_count_label.clone();
    log_buffer.connect_changed(move |_| {
        let line_count = count_log_lines(&log_buffer_clone);
        log_line_count_label_clone.set_text(&format!("{} lines", line_count));
    });

    root_box.append(&hero_card);
    root_box.append(&content_overlay);

    // Event wiring comes last to keep construction and behavior clearly separated
    wire_clear_log_button(&clear_logs_button, &log_buffer, &status_label);
    wire_hide_execution(
        &hide_panel,
        window.upcast_ref(),
        &cli_path_entry,
        &status_label,
        &log_buffer,
    );
    wire_extract_execution(&extract_panel, &cli_path_entry, &status_label, &log_buffer);

    window.set_child(Some(&root_box));
    window.present();
}
