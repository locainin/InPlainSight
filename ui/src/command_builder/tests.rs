use super::*;
use pretty_assertions::assert_eq;

fn os_args_to_strings(arguments: Vec<std::ffi::OsString>) -> Vec<String> {
    // Command builders use OsString for process safety, tests compare readable strings
    arguments
        .into_iter()
        .map(|value| value.to_string_lossy().to_string())
        .collect::<Vec<String>>()
}

#[test]
fn hide_arguments_match_expected_order() {
    // Hide must pass explicit paths and method in the order expected by the C CLI
    let hide_command = HideCommand {
        cover_path: "cover.png".to_string(),
        payload_path: "payload.bin".to_string(),
        output_path: "output.png".to_string(),
        passphrase_file_path: "pass.txt".to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert_eq!(
        os_args_to_strings(build_hide_arguments(&hide_command)),
        vec![
            "hide",
            "--cover",
            "cover.png",
            "--payload",
            "payload.bin",
            "--output",
            "output.png",
            "--passphrase-file",
            "pass.txt",
            "--method",
            "lsb"
        ]
        .into_iter()
        .map(str::to_string)
        .collect::<Vec<String>>()
    );
}

#[test]
fn hide_split_arguments_match_expected_order() {
    // Split output uses the CLI auto planner and sends both directory and template
    assert_eq!(
        os_args_to_strings(build_hide_split_arguments(
            "cover.png",
            "payload.bin",
            "out_dir",
            "hidden_payload_%04u.png",
            "pass.txt",
            EmbedMethod::Lsb
        )),
        vec![
            "hide",
            "--cover",
            "cover.png",
            "--payload",
            "payload.bin",
            "--split",
            "auto",
            "--output-dir",
            "out_dir",
            "--output-template",
            "hidden_payload_%04u.png",
            "--passphrase-file",
            "pass.txt",
            "--method",
            "lsb"
        ]
        .into_iter()
        .map(str::to_string)
        .collect::<Vec<String>>()
    );
}

#[test]
fn extract_arguments_match_expected_order() {
    // Single-image extraction must not include folder-only flags
    let extract_command = ExtractCommand {
        input_path: "stego.png".to_string(),
        input_dir: None,
        output_path: "recovered.pdf".to_string(),
        passphrase_file_path: "pass.txt".to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert_eq!(
        os_args_to_strings(build_extract_arguments(&extract_command)),
        vec![
            "extract",
            "--input",
            "stego.png",
            "--output",
            "recovered.pdf",
            "--passphrase-file",
            "pass.txt",
            "--method",
            "lsb"
        ]
        .into_iter()
        .map(str::to_string)
        .collect::<Vec<String>>()
    );
}

#[test]
fn extract_split_folder_arguments_match_expected_order() {
    // Split extraction must use the folder input flag instead of an image path
    let extract_command = ExtractCommand {
        input_path: String::new(),
        input_dir: Some("shards".to_string()),
        output_path: "recovered.pdf".to_string(),
        passphrase_file_path: "pass.txt".to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert_eq!(
        os_args_to_strings(build_extract_arguments(&extract_command)),
        vec![
            "extract",
            "--input-dir",
            "shards",
            "--output",
            "recovered.pdf",
            "--passphrase-file",
            "pass.txt",
            "--method",
            "lsb"
        ]
        .into_iter()
        .map(str::to_string)
        .collect::<Vec<String>>()
    );
}

#[test]
fn info_arguments_include_required_json_flags() {
    // Preflight depends on machine-readable JSON and the same method defaults as hide
    let info_command = InfoCommand {
        cover_path: "cover.png".to_string(),
        payload_path: Some("payload.bin".to_string()),
        payload_bytes: None,
        embed_method: EmbedMethod::Lsb,
    };

    assert_eq!(
        os_args_to_strings(build_info_arguments(&info_command)),
        vec![
            "info",
            "--cover",
            "cover.png",
            "--method",
            "lsb",
            "--lsb-bits",
            "1",
            "--density",
            "1.0",
            "--payload",
            "payload.bin",
            "--json"
        ]
        .into_iter()
        .map(str::to_string)
        .collect::<Vec<String>>()
    );
}

#[test]
fn info_arguments_support_payload_bytes_mode() {
    // Text payload preflight sends a byte count because there is no payload file yet
    let info_command = InfoCommand {
        cover_path: "cover.png".to_string(),
        payload_path: None,
        payload_bytes: Some(1234u64),
        embed_method: EmbedMethod::Lsb,
    };

    assert_eq!(
        os_args_to_strings(build_info_arguments(&info_command)),
        vec![
            "info",
            "--cover",
            "cover.png",
            "--method",
            "lsb",
            "--lsb-bits",
            "1",
            "--density",
            "1.0",
            "--payload-bytes",
            "1234",
            "--json"
        ]
        .into_iter()
        .map(str::to_string)
        .collect::<Vec<String>>()
    );
}

#[test]
fn command_runner_captures_stdout_and_exit_code() {
    // The UI needs stdout for JSON plans and the exit code for clear failure messages
    let execution_result = run_cli_command("/bin/sh", &["-c".into(), "printf 'ok'".into()])
        .expect("shell command should run");

    assert_eq!(execution_result.exit_code, Some(0));
    assert_eq!(execution_result.stdout_text, "ok");
}
