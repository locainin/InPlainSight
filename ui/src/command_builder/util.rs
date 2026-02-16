use std::path::Path;

pub fn is_regular_file(path_value: &Path) -> bool {
    path_value.is_file()
}
