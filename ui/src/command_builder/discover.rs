use std::path::PathBuf;

use crate::command_builder::util;

pub fn discover_cli_binary_path() -> Option<String> {
    let mut candidate_paths: Vec<PathBuf> = Vec::new();

    if let Ok(current_directory) = std::env::current_dir() {
        // Typical source-tree launch points: project root and ui subdirectory
        candidate_paths.push(current_directory.join("inplainsight"));
        candidate_paths.push(current_directory.join("..").join("inplainsight"));
    }

    if let Ok(current_executable) = std::env::current_exe()
        && let Some(executable_directory) = current_executable.parent()
    {
        // Typical cargo paths: ui/target/debug and neighbors
        candidate_paths.push(executable_directory.join("inplainsight"));
        candidate_paths.push(executable_directory.join("..").join("inplainsight"));
        candidate_paths.push(
            executable_directory
                .join("..")
                .join("..")
                .join("inplainsight"),
        );
        candidate_paths.push(
            executable_directory
                .join("..")
                .join("..")
                .join("..")
                .join("inplainsight"),
        );
    }

    // First match wins to keep behavior deterministic and predictable in dev
    for candidate_path in candidate_paths {
        if util::is_regular_file(&candidate_path) {
            return Some(candidate_path.to_string_lossy().to_string());
        }
    }

    None
}

pub fn build_cli_launch_candidates(cli_binary_path: &str) -> Vec<String> {
    let mut candidate_paths: Vec<String> = Vec::new();
    let trimmed_path = cli_binary_path.trim();

    // Bare names are delayed so a locally built binary is preferred over PATH lookup
    let is_bare_name = !trimmed_path.is_empty() && !trimmed_path.contains('/');
    if !trimmed_path.is_empty() && !is_bare_name {
        candidate_paths.push(trimmed_path.to_string());
    }

    // Build script points at a known-good binary path if "cargo run" built it
    if let Some(built_path) = option_env!("INPLAINSIGHT_CLI_BUILT_PATH") {
        let built_path_value = PathBuf::from(built_path);
        if util::is_regular_file(&built_path_value) {
            push_unique_path(&mut candidate_paths, built_path.to_string());
        }
    }

    // Discovery supports repo layouts where the UI is run from a subdirectory
    if let Some(discovered_path) = discover_cli_binary_path() {
        push_unique_path(&mut candidate_paths, discovered_path);
    }

    // PATH fallback comes last when a bare name was provided
    if is_bare_name {
        candidate_paths.push(trimmed_path.to_string());
    }

    candidate_paths
}

fn push_unique_path(path_list: &mut Vec<String>, path_value: String) {
    if !path_list
        .iter()
        .any(|existing_path| existing_path == &path_value)
    {
        path_list.push(path_value);
    }
}
