use std::ffi::OsString;

// Embedding mode names match the C CLI contract
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum EmbedMethod {
    Lsb,
}

impl EmbedMethod {
    pub fn as_cli_value(self) -> &'static str {
        // CLI surface is stable, so UI treats these as protocol constants
        match self {
            Self::Lsb => "lsb",
        }
    }
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct HideCommand {
    // Cover image used as carrier
    pub cover_path: String,
    // Payload path can be a real file path or an inherited /proc/self/fd path
    pub payload_path: String,
    // Destination image path
    pub output_path: String,
    // Passphrase file consumed by the C CLI
    pub passphrase_file_path: String,
    pub embed_method: EmbedMethod,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct ExtractCommand {
    // Stego image path
    pub input_path: String,
    // Output path for recovered bytes
    pub output_path: String,
    // Passphrase file consumed by the C CLI
    pub passphrase_file_path: String,
    pub embed_method: EmbedMethod,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct InfoCommand {
    // Cover image used for planning math and format decode
    pub cover_path: String,
    // Optional payload path to estimate fit and required shard count
    pub payload_path: Option<String>,
    // Optional payload length for non-file sources like typed text
    pub payload_bytes: Option<u64>,
    pub embed_method: EmbedMethod,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CommandExecution {
    // Platform-specific exit code
    pub exit_code: Option<i32>,
    // Captured standard output
    pub stdout_text: String,
    // Captured standard error
    pub stderr_text: String,
}

// Keeping OsString in this module makes it easy for callers to build args
// without needing to import std::ffi everywhere
pub type ArgumentVector = Vec<OsString>;
