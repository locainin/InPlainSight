use std::ffi::OsString;
use std::process::Command;

use crate::command_builder::discover;
use crate::command_builder::types::CommandExecution;

pub fn run_cli_command(
    cli_binary_path: &str,
    arguments: &[OsString],
) -> Result<CommandExecution, String> {
    // Candidate ordering matters for a good dev UX:
    // - explicit absolute path first (if given)
    // - built path (from build.rs)
    // - discovered sibling binary
    // - PATH fallback (for installed binaries)
    let candidate_paths = discover::build_cli_launch_candidates(cli_binary_path);
    if candidate_paths.is_empty() {
        return Err("failed to launch CLI binary: no launch candidate paths available".to_string());
    }

    let mut launch_errors: Vec<String> = Vec::new();

    for candidate_path in candidate_paths {
        // Spawn the C CLI and capture both streams for UI logging
        // This call is blocking and should run off the GTK main thread
        let output_result = Command::new(&candidate_path).args(arguments).output();
        match output_result {
            Ok(output) => {
                return Ok(CommandExecution {
                    exit_code: output.status.code(),
                    stdout_text: String::from_utf8_lossy(&output.stdout).to_string(),
                    stderr_text: String::from_utf8_lossy(&output.stderr).to_string(),
                });
            }
            Err(error_value) => {
                // NotFound is common when running from different working directories
                // Other errors (permission, exec format) usually mean the binary is not runnable
                let error_text = format!(
                    "failed to launch CLI binary '{}': {}",
                    candidate_path, error_value
                );
                let is_not_found = error_value.kind() == std::io::ErrorKind::NotFound;
                launch_errors.push(error_text);
                if !is_not_found {
                    break;
                }
            }
        }
    }

    Err(launch_errors.join("; "))
}
