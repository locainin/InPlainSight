use std::io::{Seek, SeekFrom, Write};
use std::os::fd::AsRawFd;

#[cfg(target_os = "linux")]
// Create anonymous in-memory descriptor and expose its /proc/self/fd path
pub fn create_in_memory_descriptor(
    descriptor_name: &str,
    descriptor_bytes: &[u8],
) -> Result<(String, std::fs::File), String> {
    // memfd keeps sensitive bytes off disk while still exposing a file-descriptor path
    let mut descriptor_file = memfd::MemfdOptions::default()
        .close_on_exec(false)
        .create(descriptor_name)
        .map_err(|error_value| format!("failed to create in-memory descriptor: {error_value}"))?
        .into_file();

    // Write payload or passphrase bytes into anonymous in-memory file
    descriptor_file
        .write_all(descriptor_bytes)
        .map_err(|error_value| {
            format!("failed writing bytes to in-memory descriptor: {error_value}")
        })?;

    descriptor_file
        .seek(SeekFrom::Start(0))
        .map_err(|error_value| format!("failed rewinding in-memory descriptor: {error_value}"))?;

    // Child process can read descriptor via /proc/self/fd path
    let descriptor_path = format!("/proc/self/fd/{}", descriptor_file.as_raw_fd());

    Ok((descriptor_path, descriptor_file))
}

#[cfg(not(target_os = "linux"))]
// Non-Linux targets currently do not have a memfd descriptor path implementation
pub(crate) fn create_in_memory_descriptor(
    _descriptor_name: &str,
    _descriptor_bytes: &[u8],
) -> Result<(String, std::fs::File), String> {
    Err("typed passphrase and pasted text modes currently require Linux".to_string())
}

#[cfg(test)]
mod tests {
    use std::io::Read;

    use super::create_in_memory_descriptor;

    #[cfg(target_os = "linux")]
    #[test]
    fn in_memory_descriptor_path_reads_original_bytes() {
        let expected_bytes = b"secret bytes stay in memory";
        let (descriptor_path, _descriptor_file) =
            create_in_memory_descriptor("inplainsight-test-descriptor", expected_bytes)
                .expect("descriptor should be created");

        let mut descriptor_reader =
            std::fs::File::open(descriptor_path).expect("descriptor path should remain readable");
        let mut actual_bytes = Vec::new();
        descriptor_reader
            .read_to_end(&mut actual_bytes)
            .expect("descriptor bytes should be readable");

        assert_eq!(actual_bytes, expected_bytes);
    }
}
