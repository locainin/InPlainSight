// Validate typed passphrase values before in-memory descriptor creation
pub fn validate_typed_passphrase_inputs(
    passphrase_text: &str,
    passphrase_confirm_text: Option<&str>,
) -> Result<(), String> {
    // Blank passphrases are rejected even when whitespace is present
    if passphrase_text.trim().is_empty() {
        return Err("typed passphrase is empty".to_string());
    }

    // Confirmation check is optional so extract flow can use one field
    if let Some(confirm_value) = passphrase_confirm_text
        && passphrase_text != confirm_value
    {
        return Err("typed passphrase and confirmation do not match".to_string());
    }

    Ok(())
}
