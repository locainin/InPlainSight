// Resolve and validate hide inputs
//
// This file keeps input extraction out of UI signal wiring code

use gtk4 as gtk;

use crate::app::app_types::{HidePayloadMode, PassphraseMode};
use crate::command_builder::HideCommand;
use crate::validation::{
    validate_hide_command, validate_hide_file_payload_typed_passphrase_inputs,
    validate_hide_text_inputs, validate_hide_text_typed_passphrase_inputs,
    validate_typed_passphrase_inputs,
};

use super::super::descriptor::create_in_memory_descriptor;
use super::super::helpers::extract_text_payload;
use super::types::HidePayloadResolution;

// Resolve hide payload for the chosen mode and return optional descriptor guards
pub(super) fn resolve_hide_payload(
    payload_mode: HidePayloadMode,
    passphrase_mode: PassphraseMode,
    payload_file_path: &str,
    payload_text_view: &gtk::TextView,
    cover_path: &str,
    output_path: &str,
    passphrase_file_path: &str,
) -> Result<HidePayloadResolution, String> {
    // Payload resolution supports two modes:
    // - file mode reads bytes from disk
    // - text mode keeps plaintext in memory using a descriptor-backed path
    match payload_mode {
        HidePayloadMode::File => {
            // Validation depends on how the passphrase is provided
            // Typed passphrases skip passphrase file validation on disk
            match passphrase_mode {
                PassphraseMode::File => {
                    // Full command validation covers file existence and supported extensions
                    let hide_command = HideCommand {
                        cover_path: cover_path.to_string(),
                        payload_path: payload_file_path.to_string(),
                        output_path: output_path.to_string(),
                        passphrase_file_path: passphrase_file_path.to_string(),
                        embed_method: crate::command_builder::EmbedMethod::Lsb,
                    };
                    validate_hide_command(&hide_command)?;
                }
                PassphraseMode::Text => {
                    // This validation variant avoids requiring an on-disk passphrase file
                    validate_hide_file_payload_typed_passphrase_inputs(
                        cover_path,
                        payload_file_path,
                        output_path,
                    )?;
                }
            }

            // Payload byte size is used by the planner and for UI messaging
            let payload_metadata = std::fs::metadata(payload_file_path).map_err(|error_value| {
                format!("failed reading payload metadata: {}", error_value)
            })?;

            Ok(HidePayloadResolution {
                payload_path: payload_file_path.to_string(),
                payload_log_text: payload_file_path.to_string(),
                descriptor_guards: Vec::new(),
                payload_bytes: payload_metadata.len(),
                payload_is_regular_file: true,
            })
        }
        HidePayloadMode::Text => {
            // Text payload mode keeps plaintext in memory by using an anonymous descriptor
            // This avoids writing plaintext payload data to disk
            let mut text_payload = extract_text_payload(payload_text_view)?;

            match passphrase_mode {
                PassphraseMode::File => {
                    // Text payload mode does not validate a payload file path
                    validate_hide_text_inputs(cover_path, output_path, passphrase_file_path)?;
                }
                PassphraseMode::Text => {
                    // Typed passphrase mode has no passphrase file path to validate
                    validate_hide_text_typed_passphrase_inputs(cover_path, output_path)?;
                }
            }

            // Add a trailing newline so terminal prompt characters do not appear fused to plaintext
            if !text_payload.ends_with('\n') {
                text_payload.push('\n');
            }

            // The descriptor stays open via descriptor_guards so the CLI can read the payload
            let (payload_descriptor_path, payload_file_guard) =
                create_in_memory_descriptor("inplainsight_payload", text_payload.as_bytes())?;

            Ok(HidePayloadResolution {
                payload_path: payload_descriptor_path,
                payload_log_text: format!("<pasted-text:{}-bytes>", text_payload.len()),
                descriptor_guards: vec![payload_file_guard],
                payload_bytes: text_payload.len() as u64,
                payload_is_regular_file: false,
            })
        }
    }
}

pub(super) fn resolve_hide_passphrase(
    passphrase_mode: PassphraseMode,
    passphrase_file_path: &str,
    passphrase_text: &str,
    passphrase_confirm_text: &str,
) -> Result<(String, String, Option<std::fs::File>), String> {
    // Passphrase can be sourced from:
    // - a file path on disk
    // - a typed secret kept in memory using a descriptor-backed path
    match passphrase_mode {
        PassphraseMode::File => Ok((
            passphrase_file_path.to_string(),
            // Logs avoid printing the passphrase path to reduce accidental leakage
            "<passphrase-file>".to_string(),
            None,
        )),
        PassphraseMode::Text => {
            // Typed passphrases require confirmation so typos are caught early
            validate_typed_passphrase_inputs(passphrase_text, Some(passphrase_confirm_text))?;

            // The descriptor stays open via the returned guard so the CLI can read it later
            let (passphrase_descriptor_path, passphrase_file_guard) =
                create_in_memory_descriptor("inplainsight_passphrase", passphrase_text.as_bytes())?;

            Ok((
                passphrase_descriptor_path,
                // Logs use a placeholder so secrets never appear in logs
                "<typed-passphrase>".to_string(),
                Some(passphrase_file_guard),
            ))
        }
    }
}
