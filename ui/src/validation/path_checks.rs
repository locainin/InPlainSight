use std::path::Path;

// Shared non-empty check for path fields
pub(crate) fn validate_required_path(path_value: &str, label: &str) -> Result<(), String> {
    if path_value.trim().is_empty() {
        return Err(format!("{} is required", label));
    }

    Ok(())
}

// Require existing regular file for all input paths
pub(crate) fn validate_existing_file(path_value: &str, label: &str) -> Result<(), String> {
    let candidate_path = Path::new(path_value);
    if !candidate_path.exists() {
        return Err(format!("{} does not exist", label));
    }
    if !candidate_path.is_file() {
        return Err(format!("{} is not a file", label));
    }
    Ok(())
}

// Output target must have a valid parent directory
pub(crate) fn validate_output_parent_exists(path_value: &str, label: &str) -> Result<(), String> {
    let output_path = Path::new(path_value);
    let Some(parent_directory) = output_path.parent() else {
        return Err(format!("{} has no parent directory", label));
    };

    if !parent_directory.exists() {
        return Err(format!("parent directory for {} does not exist", label));
    }
    if !parent_directory.is_dir() {
        return Err(format!("parent path for {} is not a directory", label));
    }
    Ok(())
}

// Hide supports lossless outputs that match current C backends
pub(crate) fn validate_hide_output_extension(path_value: &str) -> Result<(), String> {
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
pub(crate) fn validate_supported_image_input_extension(
    path_value: &str,
    label: &str,
) -> Result<(), String> {
    let input_path = Path::new(path_value);
    let Some(extension_value) = input_path.extension().and_then(|value| value.to_str()) else {
        return Err(format!(
            "{} must include an extension (.png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp)",
            label
        ));
    };

    let extension_lowercase = extension_value.to_ascii_lowercase();
    if is_supported_input_image_extension(&extension_lowercase) {
        return Ok(());
    }

    Err(format!(
        "{} extension is unsupported (use .png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp)",
        label
    ))
}

pub(crate) fn is_supported_output_image_extension(extension_text: &str) -> bool {
    extension_text == "png"
        || extension_text == "jxl"
        || extension_text == "bmp"
        || extension_text == "ppm"
}

pub(crate) fn is_supported_input_image_extension(extension_text: &str) -> bool {
    is_supported_output_image_extension(extension_text)
        || extension_text == "jpg"
        || extension_text == "jpeg"
        || extension_text == "webp"
}
