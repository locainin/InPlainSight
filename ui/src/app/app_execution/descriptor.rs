use std::ffi::CString;
use std::io::{Seek, SeekFrom, Write};
use std::os::fd::{AsRawFd, FromRawFd};

#[cfg(target_os = "linux")]
// Create anonymous in-memory descriptor and expose its /proc/self/fd path
pub(crate) fn create_in_memory_descriptor(
    descriptor_name: &str,
    descriptor_bytes: &[u8],
) -> Result<(String, std::fs::File), String> {
    // Descriptor names must be CString-safe for memfd_create
    let descriptor_label = CString::new(descriptor_name)
        .map_err(|_| "failed to build in-memory descriptor name".to_string())?;

    // memfd keeps sensitive bytes off disk while still exposing a file-descriptor path
    let raw_descriptor = unsafe { libc::memfd_create(descriptor_label.as_ptr(), 0) };
    if raw_descriptor < 0 {
        return Err(format!(
            "failed to create in-memory descriptor: {}",
            std::io::Error::last_os_error()
        ));
    }

    // Safe because raw_descriptor is uniquely owned here and wrapped exactly once
    let mut descriptor_file = unsafe { std::fs::File::from_raw_fd(raw_descriptor) };

    // Write payload or passphrase bytes into anonymous in-memory file
    descriptor_file
        .write_all(descriptor_bytes)
        .map_err(|error_value| {
            format!(
                "failed writing bytes to in-memory descriptor: {}",
                error_value
            )
        })?;

    descriptor_file
        .seek(SeekFrom::Start(0))
        .map_err(|error_value| format!("failed rewinding in-memory descriptor: {}", error_value))?;

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
