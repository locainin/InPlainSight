use gtk4 as gtk;

// Reusable row bundle for labeled file-path inputs
#[derive(Clone)]
pub struct FileFieldRow {
    pub container_box: gtk::Box,
    pub path_entry: gtk::Entry,
}

// Hide flow supports either file payloads or pasted text payloads
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum HidePayloadMode {
    File,
    Text,
}

// Passphrase can come from a file or typed text
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PassphraseMode {
    File,
    Text,
}

// Widgets owned by the hide panel
pub struct HidePanel {
    pub cover_field: FileFieldRow,
    pub payload_file_field: FileFieldRow,
    pub output_field: FileFieldRow,
    pub passphrase_file_field: FileFieldRow,
    pub payload_source_dropdown: gtk::DropDown,
    pub payload_stack: gtk::Stack,
    pub payload_text_view: gtk::TextView,
    pub passphrase_source_dropdown: gtk::DropDown,
    pub passphrase_stack: gtk::Stack,
    pub passphrase_text_entry: gtk::PasswordEntry,
    pub passphrase_confirm_entry: gtk::PasswordEntry,
    pub method_dropdown: gtk::DropDown,
    pub run_button: gtk::Button,
}

// Widgets owned by the extract panel
pub struct ExtractPanel {
    pub input_field: FileFieldRow,
    pub output_field: FileFieldRow,
    pub passphrase_text_entry: gtk::PasswordEntry,
    pub run_button: gtk::Button,
}
