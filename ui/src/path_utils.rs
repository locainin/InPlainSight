use std::path::{Path, PathBuf};

pub fn expand_home_path(path_text: &str) -> PathBuf {
    let trimmed_path = path_text.trim();

    // Only current-user ~/ paths are expanded
    // ~other stays literal because guessing another account path is not reliable
    if (trimmed_path == "~" || trimmed_path.starts_with("~/"))
        && let Some(home_dir) = std::env::var_os("HOME")
    {
        let mut expanded_path = PathBuf::from(home_dir);
        if trimmed_path.len() > 2 {
            expanded_path.push(&trimmed_path[2..]);
        }
        return expanded_path;
    }

    PathBuf::from(trimmed_path)
}

pub fn compact_home_path(path_value: &Path) -> String {
    let path_text = path_value.to_string_lossy().to_string();

    // Home-relative display keeps screenshots and summaries from exposing the account path
    let Some(home_dir) = std::env::var_os("HOME").map(PathBuf::from) else {
        return path_text;
    };
    if path_value == home_dir {
        return "~".to_string();
    }
    path_value
        .strip_prefix(&home_dir)
        .map_or(path_text, |stripped_path| {
            let stripped_text = stripped_path.to_string_lossy();
            if stripped_text.is_empty() {
                "~".to_string()
            } else {
                format!("~/{stripped_text}")
            }
        })
}

pub fn compact_home_text(path_text: &str) -> String {
    let trimmed_path = path_text.trim();
    if trimmed_path.is_empty() {
        return String::new();
    }

    compact_home_path(&expand_home_path(trimmed_path))
}

#[cfg(test)]
mod tests {
    use super::{compact_home_path, expand_home_path};

    #[test]
    fn expand_home_path_accepts_short_home_prefix() {
        let Some(home_dir) = std::env::var_os("HOME").map(std::path::PathBuf::from) else {
            return;
        };

        assert_eq!(
            expand_home_path("~/Pictures/file.png"),
            home_dir.join("Pictures/file.png")
        );
    }

    #[test]
    fn compact_home_path_hides_local_account_prefix() {
        let Some(home_dir) = std::env::var_os("HOME").map(std::path::PathBuf::from) else {
            return;
        };

        assert_eq!(
            compact_home_path(&home_dir.join("Downloads/out.png")),
            "~/Downloads/out.png"
        );
    }
}
