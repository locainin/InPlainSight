use std::path::PathBuf;

use crate::command_builder::discover;
use crate::command_builder::util;
use crate::path_utils::compact_home_path;

pub fn default_cli_binary_path() -> String {
    // Allow explicit override for custom layouts and packaging
    if let Ok(configured_path) = std::env::var("INPLAINSIGHT_CLI_PATH") {
        let trimmed_path = configured_path.trim();
        if !trimmed_path.is_empty() {
            return trimmed_path.to_string();
        }
    }

    // Build script sets this when cargo compiled the C binary successfully
    if let Some(built_path) = option_env!("INPLAINSIGHT_CLI_BUILT_PATH") {
        let built_path_value = PathBuf::from(built_path);
        if util::is_regular_file(&built_path_value) {
            return built_path.to_string();
        }
    }

    // Prefer an absolute discovered path so launch context does not break CLI lookup
    if let Some(discovered_path) = discover::discover_cli_binary_path() {
        return discovered_path;
    }

    // Fallback keeps behavior predictable when binary is not built yet
    "inplainsight".to_string()
}

// Hide output defaults to Downloads so a valid destination exists before interaction
pub fn default_hide_output_path() -> String {
    let downloads_dir = gtk4::glib::user_special_dir(gtk4::glib::UserDirectory::Downloads)
        .or_else(|| {
            std::env::var_os("HOME")
                .map(PathBuf::from)
                .map(|home_dir| home_dir.join("Downloads"))
        })
        .unwrap_or_else(|| PathBuf::from("."));
    compact_home_path(&downloads_dir.join("hidden_payload.png"))
}

// Extract output defaults to Downloads for a familiar desktop workflow
pub fn default_extract_output_path() -> String {
    let downloads_dir = gtk4::glib::user_special_dir(gtk4::glib::UserDirectory::Downloads)
        .or_else(|| {
            std::env::var_os("HOME")
                .map(PathBuf::from)
                .map(|home_dir| home_dir.join("Downloads"))
        })
        .unwrap_or_else(|| PathBuf::from("."));
    compact_home_path(&downloads_dir.join("recovered_payload.bin"))
}
