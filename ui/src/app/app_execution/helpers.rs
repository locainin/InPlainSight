use std::path::Path;

use gtk::prelude::*;
use gtk4 as gtk;

pub(crate) fn normalize_hide_output_path(
    cover_path: &str,
    output_path: &str,
) -> Result<String, String> {
    // Whitespace-only output input is treated as empty so caller can decide defaults
    let trimmed_output = output_path.trim();
    if trimmed_output.is_empty() {
        return Ok(String::new());
    }

    // Preserve explicit extension when user already provided one
    let output_path_object = Path::new(trimmed_output);
    if output_path_object.extension().is_some() {
        return Ok(trimmed_output.to_string());
    }

    let cover_extension = Path::new(cover_path)
        .extension()
        .and_then(|value| value.to_str())
        .map(|value| value.to_ascii_lowercase())
        .ok_or_else(|| {
            "cover image extension is required to infer output format (.png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp)"
                .to_string()
        })?;

    if !is_supported_input_cover_extension(&cover_extension) {
        return Err(
            "unsupported cover extension for output inference (use .png, .jxl, .bmp, .ppm, .jpg, .jpeg, .webp)"
                .to_string(),
        );
    }

    // Lossy cover formats decode correctly but output defaults to PNG so bits remain stable
    let inferred_extension = if is_supported_lossless_output_extension(&cover_extension) {
        cover_extension
    } else {
        "png".to_string()
    };

    // Return normalized value with inferred extension appended
    Ok(format!("{}.{}", trimmed_output, inferred_extension))
}

pub(crate) fn is_supported_lossless_output_extension(extension_text: &str) -> bool {
    extension_text == "png"
        || extension_text == "jxl"
        || extension_text == "bmp"
        || extension_text == "ppm"
}

pub(crate) fn is_supported_input_cover_extension(extension_text: &str) -> bool {
    is_supported_lossless_output_extension(extension_text)
        || extension_text == "jpg"
        || extension_text == "jpeg"
        || extension_text == "webp"
}

// Read text payload from text view and reject empty values
pub(crate) fn extract_text_payload(payload_text_view: &gtk::TextView) -> Result<String, String> {
    // Read full text buffer from start to end for exact payload capture
    let payload_text_buffer = payload_text_view.buffer();
    let start_iter = payload_text_buffer.start_iter();
    let end_iter = payload_text_buffer.end_iter();
    let text_payload = payload_text_buffer
        .text(&start_iter, &end_iter, false)
        .to_string();

    if text_payload.trim().is_empty() {
        return Err("text payload is empty".to_string());
    }

    Ok(text_payload)
}
