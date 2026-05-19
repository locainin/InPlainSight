use std::path::Path;

use crate::path_utils::expand_home_path;

// Shared non-empty check for path fields
pub fn validate_required_path(path_value: &str, label: &str) -> Result<(), String> {
    if path_value.trim().is_empty() {
        return Err(format!("{label} is required"));
    }

    Ok(())
}

// Require existing regular file for all input paths
pub fn validate_existing_file(path_value: &str, label: &str) -> Result<(), String> {
    let candidate_path = expand_home_path(path_value);
    if !candidate_path.exists() {
        return Err(format!("{label} does not exist"));
    }
    if !candidate_path.is_file() {
        return Err(format!("{label} is not a file"));
    }
    Ok(())
}

// Require an existing directory for folder-based CLI inputs
pub fn validate_existing_directory(path_value: &str, label: &str) -> Result<(), String> {
    let candidate_path = expand_home_path(path_value);
    if !candidate_path.exists() {
        return Err(format!("{label} does not exist"));
    }
    if !candidate_path.is_dir() {
        return Err(format!("{label} is not a directory"));
    }
    Ok(())
}

// Output target must have a valid parent directory
pub fn validate_output_parent_exists(path_value: &str, label: &str) -> Result<(), String> {
    let output_path = expand_home_path(path_value);
    let Some(parent_directory) = output_path.parent() else {
        return Err(format!("{label} has no parent directory"));
    };

    if !parent_directory.exists() {
        return Err(format!("parent directory for {label} does not exist"));
    }
    if !parent_directory.is_dir() {
        return Err(format!("parent path for {label} is not a directory"));
    }
    Ok(())
}

// Hide supports lossless outputs that match current C backends
pub fn validate_hide_output_extension(path_value: &str) -> Result<(), String> {
    let output_path = Path::new(path_value);
    let Some(extension_value) = output_path.extension().and_then(|value| value.to_str()) else {
        return Err("output path must include an extension (.png, .jxl, .bmp, .ppm)".to_string());
    };

    let extension_lowercase = extension_value.to_ascii_lowercase();
    if is_supported_output_image_extension(&extension_lowercase) {
        return Ok(());
    }

    Err("unsupported output image extension (use .png, .jxl, .bmp, .ppm)".to_string())
}

// Input image must use a supported extension
pub fn validate_supported_image_input_extension(
    path_value: &str,
    label: &str,
) -> Result<(), String> {
    let input_path = Path::new(path_value);
    let Some(extension_value) = input_path.extension().and_then(|value| value.to_str()) else {
        return Err(format!(
            "{label} must include an extension (.png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp)"
        ));
    };

    let extension_lowercase = extension_value.to_ascii_lowercase();
    if is_supported_input_image_extension(&extension_lowercase) {
        return Ok(());
    }

    Err(format!(
        "{label} extension is unsupported (use .png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp)"
    ))
}

pub fn is_supported_output_image_extension(extension_text: &str) -> bool {
    extension_text == "png"
        || extension_text == "jxl"
        || extension_text == "bmp"
        || extension_text == "ppm"
}

pub fn is_supported_input_image_extension(extension_text: &str) -> bool {
    is_supported_output_image_extension(extension_text)
        || extension_text == "jpg"
        || extension_text == "jpeg"
        || extension_text == "webp"
}
