// Validation module keeps user input checks isolated from UI wiring logic
mod extract_checks;
mod hide_checks;
mod passphrase_checks;
mod path_checks;

#[cfg(test)]
mod tests;

// Re-export entry points so callers use one small validation surface
pub use extract_checks::validate_extract_folder_typed_passphrase_inputs;
pub use extract_checks::validate_extract_typed_passphrase_inputs;
pub use hide_checks::{
    validate_hide_command, validate_hide_file_payload_typed_passphrase_inputs,
    validate_hide_text_inputs, validate_hide_text_typed_passphrase_inputs,
};
pub use passphrase_checks::validate_typed_passphrase_inputs;
