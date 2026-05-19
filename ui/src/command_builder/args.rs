use crate::command_builder::types::{
    ArgumentVector, EmbedMethod, ExtractCommand, HideCommand, InfoCommand,
};

pub fn build_hide_arguments(command: &HideCommand) -> ArgumentVector {
    // Keep argument order stable for tests and log readability
    vec![
        "hide".into(),
        "--cover".into(),
        command.cover_path.clone().into(),
        "--payload".into(),
        command.payload_path.clone().into(),
        "--output".into(),
        command.output_path.clone().into(),
        "--passphrase-file".into(),
        command.passphrase_file_path.clone().into(),
        "--method".into(),
        command.embed_method.as_cli_value().into(),
    ]
}

pub fn build_hide_split_arguments(
    cover_path: &str,
    payload_path: &str,
    output_dir: &str,
    output_template: &str,
    passphrase_file_path: &str,
    embed_method: EmbedMethod,
) -> ArgumentVector {
    // Split mode writes multiple shard images into one output directory
    vec![
        "hide".into(),
        "--cover".into(),
        cover_path.into(),
        "--payload".into(),
        payload_path.into(),
        "--split".into(),
        "auto".into(),
        "--output-dir".into(),
        output_dir.into(),
        "--output-template".into(),
        output_template.into(),
        "--passphrase-file".into(),
        passphrase_file_path.into(),
        "--method".into(),
        embed_method.as_cli_value().into(),
    ]
}

pub fn build_extract_arguments(command: &ExtractCommand) -> ArgumentVector {
    // Keep argument order stable for tests and log readability
    let mut arguments: ArgumentVector = vec!["extract".into()];
    if let Some(input_dir) = &command.input_dir {
        arguments.push("--input-dir".into());
        arguments.push(input_dir.clone().into());
    } else {
        arguments.push("--input".into());
        arguments.push(command.input_path.clone().into());
    }
    arguments.extend([
        "--output".into(),
        command.output_path.clone().into(),
        "--passphrase-file".into(),
        command.passphrase_file_path.clone().into(),
        "--method".into(),
        command.embed_method.as_cli_value().into(),
    ]);
    arguments
}

pub fn build_info_arguments(command: &InfoCommand) -> ArgumentVector {
    let mut arguments: ArgumentVector = vec![
        "info".into(),
        "--cover".into(),
        command.cover_path.clone().into(),
        "--method".into(),
        command.embed_method.as_cli_value().into(),
        // Info uses the same lossless image path that hide execution will use
        "--lsb-bits".into(),
        "1".into(),
        "--density".into(),
        "1.0".into(),
    ];

    if let Some(payload_path) = &command.payload_path {
        arguments.push("--payload".into());
        arguments.push(payload_path.clone().into());
    }
    if let Some(payload_bytes) = command.payload_bytes {
        arguments.push("--payload-bytes".into());
        arguments.push(payload_bytes.to_string().into());
    }

    arguments.push("--json".into());
    arguments
}
