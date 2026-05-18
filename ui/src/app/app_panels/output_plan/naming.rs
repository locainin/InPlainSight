pub fn preview_name_from_pattern(pattern_text: &str, shard_index: usize) -> String {
    let formatted_index = format!("{shard_index:04}");
    let trimmed_pattern = pattern_text.trim();
    if trimmed_pattern.is_empty() {
        return format!("hidden_payload_part_{formatted_index}.png");
    }
    if trimmed_pattern.contains("{index}") {
        return trimmed_pattern.replace("{index}", &formatted_index);
    }
    if trimmed_pattern.contains("{i}") {
        return trimmed_pattern.replace("{i}", &formatted_index);
    }
    if trimmed_pattern.contains("%04u") {
        return trimmed_pattern.replace("%04u", &formatted_index);
    }
    if trimmed_pattern.contains("%u") {
        return trimmed_pattern.replace("%u", &shard_index.to_string());
    }
    trimmed_pattern.to_string()
}
