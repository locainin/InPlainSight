use super::path_checks::{
    validate_existing_file, validate_output_parent_exists, validate_required_path,
    validate_supported_image_input_extension,
};

// Validate extract command fields when typed passphrase mode is used
pub fn validate_extract_typed_passphrase_inputs(
    input_path: &str,
    output_path: &str,
) -> Result<(), String> {
    // Required checks keep empty form submissions out of command execution
    validate_required_path(input_path, "input path")?;
    validate_required_path(output_path, "output path")?;
    // Input must already exist because extract never creates source files
    validate_existing_file(input_path, "input path")?;
    // Restrict to known image extensions accepted by the CLI layer
    validate_supported_image_input_extension(input_path, "input path")?;
    // Parent path must exist so output write will not fail at runtime
    validate_output_parent_exists(output_path, "output path")?;

    // Output overwrite of input is blocked to avoid destructive mistakes
    if input_path == output_path {
        return Err("output path must be different from input path".to_string());
    }

    Ok(())
}
