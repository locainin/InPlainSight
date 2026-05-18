use std::fs::{self, File};
use std::path::{Path, PathBuf};
use std::time::{SystemTime, UNIX_EPOCH};

use crate::command_builder::{EmbedMethod, HideCommand};

use super::{
    validate_extract_folder_typed_passphrase_inputs, validate_extract_typed_passphrase_inputs,
    validate_hide_command, validate_hide_file_payload_typed_passphrase_inputs,
    validate_hide_text_inputs, validate_hide_text_typed_passphrase_inputs,
    validate_typed_passphrase_inputs,
};

fn unique_temp_directory() -> PathBuf {
    let timestamp_nanos = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .expect("time should move forward")
        .as_nanos();
    let directory_path = std::env::temp_dir().join(format!(
        "inplainsight_ui_validation_test_{}_{}",
        std::process::id(),
        timestamp_nanos
    ));
    fs::create_dir_all(&directory_path).expect("temp directory should be creatable");
    directory_path
}

fn create_test_file(path_value: &Path) {
    let _file_handle = File::create(path_value).expect("test file should be creatable");
}

#[test]
fn hide_validation_rejects_missing_paths() {
    let command = HideCommand {
        cover_path: String::new(),
        payload_path: "payload.bin".to_string(),
        output_path: "output.png".to_string(),
        passphrase_file_path: "pass.txt".to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert!(validate_hide_command(&command).is_err());
}

#[test]
fn hide_validation_rejects_same_cover_and_output() {
    let command = HideCommand {
        cover_path: "same.png".to_string(),
        payload_path: "payload.bin".to_string(),
        output_path: "same.png".to_string(),
        passphrase_file_path: "pass.txt".to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert!(validate_hide_command(&command).is_err());
}

#[test]
fn hide_validation_rejects_missing_payload_file() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.png");
    let passphrase_path = test_root_directory.join("passphrase.txt");
    let output_path = test_root_directory.join("output.png");

    create_test_file(&cover_path);
    create_test_file(&passphrase_path);

    let command = HideCommand {
        cover_path: cover_path.to_string_lossy().to_string(),
        payload_path: test_root_directory
            .join("missing_payload.bin")
            .to_string_lossy()
            .to_string(),
        output_path: output_path.to_string_lossy().to_string(),
        passphrase_file_path: passphrase_path.to_string_lossy().to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert!(validate_hide_command(&command).is_err());
}

#[test]
fn hide_validation_rejects_output_without_extension() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.png");
    let payload_path = test_root_directory.join("payload.bin");
    let passphrase_path = test_root_directory.join("passphrase.txt");
    let output_path = test_root_directory.join("output_without_extension");

    create_test_file(&cover_path);
    create_test_file(&payload_path);
    create_test_file(&passphrase_path);

    let command = HideCommand {
        cover_path: cover_path.to_string_lossy().to_string(),
        payload_path: payload_path.to_string_lossy().to_string(),
        output_path: output_path.to_string_lossy().to_string(),
        passphrase_file_path: passphrase_path.to_string_lossy().to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert!(validate_hide_command(&command).is_err());
}

#[test]
fn hide_validation_rejects_unsupported_cover_extension() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.gif");
    let payload_path = test_root_directory.join("payload.bin");
    let passphrase_path = test_root_directory.join("passphrase.txt");
    let output_path = test_root_directory.join("output.png");

    create_test_file(&cover_path);
    create_test_file(&payload_path);
    create_test_file(&passphrase_path);

    let command = HideCommand {
        cover_path: cover_path.to_string_lossy().to_string(),
        payload_path: payload_path.to_string_lossy().to_string(),
        output_path: output_path.to_string_lossy().to_string(),
        passphrase_file_path: passphrase_path.to_string_lossy().to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert!(validate_hide_command(&command).is_err());
}

#[test]
fn hide_validation_accepts_jpeg_cover_when_output_is_lossless() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.jpeg");
    let payload_path = test_root_directory.join("payload.bin");
    let passphrase_path = test_root_directory.join("passphrase.txt");
    let output_path = test_root_directory.join("output.png");

    create_test_file(&cover_path);
    create_test_file(&payload_path);
    create_test_file(&passphrase_path);

    let command = HideCommand {
        cover_path: cover_path.to_string_lossy().to_string(),
        payload_path: payload_path.to_string_lossy().to_string(),
        output_path: output_path.to_string_lossy().to_string(),
        passphrase_file_path: passphrase_path.to_string_lossy().to_string(),
        embed_method: EmbedMethod::Lsb,
    };

    assert!(validate_hide_command(&command).is_ok());
}

#[test]
fn hide_text_validation_accepts_valid_inputs() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.png");
    let passphrase_path = test_root_directory.join("passphrase.txt");
    let output_path = test_root_directory.join("stego.png");

    create_test_file(&cover_path);
    create_test_file(&passphrase_path);

    let validation_result = validate_hide_text_inputs(
        &cover_path.to_string_lossy(),
        &output_path.to_string_lossy(),
        &passphrase_path.to_string_lossy(),
    );

    assert!(validation_result.is_ok());
}

#[test]
fn hide_typed_passphrase_validation_accepts_valid_file_payload_inputs() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.png");
    let payload_path = test_root_directory.join("payload.bin");
    let output_path = test_root_directory.join("stego.png");

    create_test_file(&cover_path);
    create_test_file(&payload_path);

    let validation_result = validate_hide_file_payload_typed_passphrase_inputs(
        &cover_path.to_string_lossy(),
        &payload_path.to_string_lossy(),
        &output_path.to_string_lossy(),
    );

    assert!(validation_result.is_ok());
}

#[test]
fn typed_passphrase_validation_rejects_mismatch() {
    let validation_result = validate_typed_passphrase_inputs("secret-a", Some("secret-b"));
    assert!(validation_result.is_err());
}

#[test]
fn extract_typed_passphrase_validation_accepts_valid_paths() {
    let test_root_directory = unique_temp_directory();
    let input_path = test_root_directory.join("stego.png");
    let output_path = test_root_directory.join("output.bin");

    create_test_file(&input_path);

    let validation_result = validate_extract_typed_passphrase_inputs(
        &input_path.to_string_lossy(),
        &output_path.to_string_lossy(),
    );
    assert!(validation_result.is_ok());
}

#[test]
fn extract_folder_typed_passphrase_validation_accepts_existing_folder() {
    let test_root_directory = unique_temp_directory();
    let input_dir = test_root_directory.join("shards");
    let output_path = test_root_directory.join("recovered.bin");
    fs::create_dir_all(&input_dir).expect("input dir should be creatable");

    let validation_result = validate_extract_folder_typed_passphrase_inputs(
        &input_dir.to_string_lossy(),
        &output_path.to_string_lossy(),
    );

    assert!(validation_result.is_ok());
}

#[test]
fn hide_text_typed_passphrase_validation_accepts_valid_paths() {
    let test_root_directory = unique_temp_directory();
    let cover_path = test_root_directory.join("cover.png");
    let output_path = test_root_directory.join("stego.png");

    create_test_file(&cover_path);

    let validation_result = validate_hide_text_typed_passphrase_inputs(
        &cover_path.to_string_lossy(),
        &output_path.to_string_lossy(),
    );
    assert!(validation_result.is_ok());
}
