use crate::command_builder::HideCommand;

use super::path_checks::{
    validate_existing_file, validate_hide_output_extension, validate_output_parent_exists,
    validate_required_path, validate_supported_image_input_extension,
};

// Validate full hide command in file-payload mode
pub fn validate_hide_command(command: &HideCommand) -> Result<(), String> {
    validate_required_path(&command.cover_path, "cover path")?;
    validate_required_path(&command.payload_path, "payload path")?;
    validate_required_path(&command.output_path, "output path")?;
    validate_required_path(&command.passphrase_file_path, "passphrase file")?;
    validate_existing_file(&command.cover_path, "cover path")?;
    validate_existing_file(&command.payload_path, "payload path")?;
    validate_existing_file(&command.passphrase_file_path, "passphrase file")?;
    validate_supported_image_input_extension(&command.cover_path, "cover path")?;
    validate_output_parent_exists(&command.output_path, "output path")?;
    validate_hide_output_extension(&command.output_path)?;

    if command.cover_path == command.output_path {
        return Err("output path must be different from cover path".to_string());
    }

    Ok(())
}

// Validate hide command fields when payload comes from pasted text
pub fn validate_hide_text_inputs(
    cover_path: &str,
    output_path: &str,
    passphrase_file_path: &str,
) -> Result<(), String> {
    validate_required_path(cover_path, "cover path")?;
    validate_required_path(output_path, "output path")?;
    validate_required_path(passphrase_file_path, "passphrase file")?;
    validate_existing_file(cover_path, "cover path")?;
    validate_existing_file(passphrase_file_path, "passphrase file")?;
    validate_supported_image_input_extension(cover_path, "cover path")?;
    validate_output_parent_exists(output_path, "output path")?;
    validate_hide_output_extension(output_path)?;

    if cover_path == output_path {
        return Err("output path must be different from cover path".to_string());
    }

    Ok(())
}

// Validate hide command fields when payload file is used with typed passphrase
pub fn validate_hide_file_payload_typed_passphrase_inputs(
    cover_path: &str,
    payload_path: &str,
    output_path: &str,
) -> Result<(), String> {
    validate_required_path(cover_path, "cover path")?;
    validate_required_path(payload_path, "payload path")?;
    validate_required_path(output_path, "output path")?;
    validate_existing_file(cover_path, "cover path")?;
    validate_existing_file(payload_path, "payload path")?;
    validate_supported_image_input_extension(cover_path, "cover path")?;
    validate_output_parent_exists(output_path, "output path")?;
    validate_hide_output_extension(output_path)?;

    if cover_path == output_path {
        return Err("output path must be different from cover path".to_string());
    }

    Ok(())
}

// Validate hide command fields when payload text and typed passphrase are used
pub fn validate_hide_text_typed_passphrase_inputs(
    cover_path: &str,
    output_path: &str,
) -> Result<(), String> {
    validate_required_path(cover_path, "cover path")?;
    validate_required_path(output_path, "output path")?;
    validate_existing_file(cover_path, "cover path")?;
    validate_supported_image_input_extension(cover_path, "cover path")?;
    validate_output_parent_exists(output_path, "output path")?;
    validate_hide_output_extension(output_path)?;

    if cover_path == output_path {
        return Err("output path must be different from cover path".to_string());
    }

    Ok(())
}
