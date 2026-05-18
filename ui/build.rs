use std::env;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

// Build the C CLI whenever the UI is built so cargo run works out of the box
fn main() {
    // Cargo does not re-run build scripts on env changes unless told explicitly
    println!("cargo:rerun-if-env-changed=INPLAINSIGHT_SKIP_CLI_BUILD");

    let manifest_dir = match env::var("CARGO_MANIFEST_DIR") {
        Ok(value) => PathBuf::from(value),
        Err(error) => panic!("CARGO_MANIFEST_DIR is not set: {error}"),
    };

    // UI crate lives under ui/ in this repo, so the parent is usually the project root
    // A Makefile presence check avoids silently pointing at the wrong directory if layout changes
    let parent_dir = manifest_dir
        .parent()
        .map_or_else(|| manifest_dir.clone(), Path::to_path_buf);
    let project_root = if parent_dir.join("Makefile").is_file() {
        parent_dir
    } else if manifest_dir.join("Makefile").is_file() {
        manifest_dir
    } else {
        panic!(
            "failed to locate project root: Makefile not found at '{}' or '{}'",
            parent_dir.display(),
            manifest_dir.display()
        );
    };

    emit_rerun_if_changed(&project_root.join("Makefile"));
    emit_rerun_if_changed(&project_root.join("include"));
    emit_rerun_if_changed(&project_root.join("src"));

    let skip_build = env::var("INPLAINSIGHT_SKIP_CLI_BUILD")
        .ok()
        .is_some_and(|value_text| value_text == "1");

    if skip_build {
        return;
    }

    let cli_binary_path = project_root.join("inplainsight");
    let build_result = Command::new("make")
        .arg("-C")
        .arg(&project_root)
        .arg("inplainsight")
        .status();

    match build_result {
        Ok(status) if status.success() => {
            println!(
                "cargo:rustc-env=INPLAINSIGHT_CLI_BUILT_PATH={}",
                cli_binary_path.display()
            );
        }
        Ok(status) => {
            panic!(
                "failed to build C CLI via make (exit status: {status})\n\
                 ensure 'make' is installed and the C dependencies are available",
            );
        }
        Err(error) => {
            panic!(
                "failed to run make for C CLI build: {error}\n\
                 ensure 'make' is installed and available on PATH",
            );
        }
    }
}

fn emit_rerun_if_changed(path_value: &Path) {
    if path_value.is_file() {
        println!("cargo:rerun-if-changed={}", path_value.display());
        return;
    }

    if !path_value.is_dir() {
        return;
    }

    let Ok(entries) = fs::read_dir(path_value) else {
        return;
    };

    for entry_result in entries {
        let Ok(entry_value) = entry_result else {
            continue;
        };
        let child_path = entry_value.path();
        if child_path.is_dir() {
            emit_rerun_if_changed(&child_path);
        } else if child_path.is_file() {
            println!("cargo:rerun-if-changed={}", child_path.display());
        }
    }
}
