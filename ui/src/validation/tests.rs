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
    // Each test gets its own folder so file-existence checks never share state
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
    // Empty files are enough because validation only checks path safety and presence
    let _file_handle = File::create(path_value).expect("test file should be creatable");
}

#[test]
fn hide_validation_rejects_missing_paths() {
    // Empty required paths must fail before any process execution is attempted
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
    // Writing over the cover would destroy the original carrier image
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
    // Missing payload files fail fast so the CLI never runs with a broken path
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
    // Output extension is required because the CLI format follows the file name
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
    // Unsupported carrier formats fail in validation instead of producing a CLI error dialog
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
    // JPEG is valid as an input cover while the generated output remains lossless PNG
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
    // Text payload mode only needs cover, output, and a passphrase source
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
    // Typed passphrase mode removes the passphrase file requirement
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
    // Confirm field must match before a sensitive operation can run
    let validation_result = validate_typed_passphrase_inputs("secret-a", Some("secret-b"));
    assert!(validation_result.is_err());
}

#[test]
fn extract_typed_passphrase_validation_accepts_valid_paths() {
    // Single-image extract checks that the stego image exists and output is writable by path
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
    // Split extract starts from a folder of generated images
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
    // Text hide with typed passphrase validates the same output safety path as file payloads
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
