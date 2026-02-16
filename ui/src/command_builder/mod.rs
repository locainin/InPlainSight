// Central "command builder" module
//
// This module is the UI's compatibility layer around the C CLI
// It owns:
// - stable Rust types used by the rest of the UI
// - default paths (Downloads, CLI binary lookup)
// - argv construction as OsString (safe for non-UTF8 paths)
// - a small blocking runner that captures stdout/stderr for the log panel
//
// It does not own GTK widgets or any UI state
// Callers should run the blocking runner off the GTK main thread

mod args;
mod defaults;
mod discover;
mod runner;
mod types;
mod util;

pub use args::{
    build_extract_arguments, build_hide_arguments, build_hide_split_arguments, build_info_arguments,
};
pub use defaults::{
    default_cli_binary_path, default_extract_output_path, default_hide_output_path,
};
pub use runner::run_cli_command;
pub use types::{CommandExecution, EmbedMethod, ExtractCommand, HideCommand, InfoCommand};

#[cfg(test)]
mod tests;
