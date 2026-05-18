// App modules are grouped by visible area, execution flow, and shared input state
mod app_chrome;
mod app_execution;
mod app_fields;
mod app_logging;
mod app_panels;
mod app_types;
pub mod app_ui_helpers;
mod app_window;

// GTK activates through this single exported builder
pub use app_window::build_ui;
