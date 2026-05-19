pub fn preview_name_from_pattern(pattern_text: &str, shard_index: usize) -> String {
    // Keep preview numbering aligned with the CLI's zero-padded shard naming
    let formatted_index = format!("{shard_index:04}");
    let trimmed_pattern = pattern_text.trim();
    if trimmed_pattern.is_empty() {
        // Empty input still needs a stable preview so the table never looks broken
        return format!("hidden_payload_part_{formatted_index}.png");
    }
    if trimmed_pattern.contains("{index}") {
        // Friendly token used by the GUI copy
        return trimmed_pattern.replace("{index}", &formatted_index);
    }
    if trimmed_pattern.contains("{i}") {
        // Short token kept for compact patterns
        return trimmed_pattern.replace("{i}", &formatted_index);
    }
    if trimmed_pattern.contains("%04u") {
        // CLI-style token with fixed padding
        return trimmed_pattern.replace("%04u", &formatted_index);
    }
    if trimmed_pattern.contains("%u") {
        // CLI-style token without padding
        return trimmed_pattern.replace("%u", &shard_index.to_string());
    }
    // Literal names are shown as typed because validation happens before execution
    trimmed_pattern.to_string()
}
