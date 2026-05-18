use std::path::Path;

pub fn is_regular_file(path_value: &Path) -> bool {
    // Keep file checks centralized so command validation and tests use the same rule
    path_value.is_file()
}
