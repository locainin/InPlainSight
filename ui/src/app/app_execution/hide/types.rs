// Shared types for the hide workflow
//
// GTK widgets are cloned and passed around by value in this module
// Cloning GTK objects is cheap because it only bumps a reference count

use gtk::prelude::*;
use gtk4 as gtk;

use crate::command_builder::HideCommand;

// Resolved payload inputs used for both hide command and preflight planning
pub(super) struct HidePayloadResolution {
    // Payload path passed to the C CLI
    // This can be a real file path or a descriptor-backed path
    pub(super) payload_path: String,
    // Log-safe payload display string
    // This avoids putting descriptor paths into logs
    pub(super) payload_log_text: String,
    // Open files kept alive so descriptor paths stay valid during execution
    pub(super) descriptor_guards: Vec<std::fs::File>,
    // Payload length used for planning when payload is not a regular file path
    pub(super) payload_bytes: u64,
    // When true, planner uses payload_path and reads metadata from disk
    pub(super) payload_is_regular_file: bool,
}

// UI handles used across preflight and the final hide run
pub(super) struct HidePreflightUi {
    pub(super) parent_window: gtk::Window,
    pub(super) run_button: gtk::Button,
    pub(super) status_label: gtk::Label,
    pub(super) log_buffer: gtk::TextBuffer,
}

// Immutable inputs for the hide operation once validation has passed
pub(super) struct HideExecutionInputs {
    pub(super) cli_binary_path: String,
    pub(super) hide_command: HideCommand,
    pub(super) payload_log_text: String,
    pub(super) passphrase_log_text: String,
    pub(super) descriptor_guards: Vec<std::fs::File>,
    pub(super) payload_bytes: u64,
    pub(super) payload_is_regular_file: bool,
}

// Status label styling is used in many branches, so keep it centralized
pub(super) fn set_status_ready(status_label: &gtk::Label, text: &str) {
    status_label.set_text(text);
    status_label.add_css_class("status-ready");
    status_label.remove_css_class("status-ok");
    status_label.remove_css_class("status-fail");
}

pub(super) fn set_status_fail(status_label: &gtk::Label, text: &str) {
    status_label.set_text(text);
    status_label.remove_css_class("status-ready");
    status_label.remove_css_class("status-ok");
    status_label.add_css_class("status-fail");
}
