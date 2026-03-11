//! Safe Rust wrapper around the libsharedmemory C FFI.
//!
//! # Example
//!
//! ```no_run
//! use libsharedmemory::SharedMemory;
//!
//! let writer = SharedMemory::create("rust_demo", 256, true).unwrap();
//! writer.as_mut_slice()[..5].copy_from_slice(b"hello");
//!
//! let reader = SharedMemory::open("rust_demo", 256, true).unwrap();
//! assert_eq!(&reader.as_slice()[..5], b"hello");
//! ```

mod ffi;

use std::ffi::CString;
use std::fmt;
use std::ptr::NonNull;

/// A handle to a shared memory segment.
///
/// Dropping the handle closes the mapping. Call [`SharedMemory::destroy`] to
/// also unlink the segment from the OS so it can be reclaimed.
pub struct SharedMemory {
    inner: NonNull<ffi::lsm_memory>,
    size: usize,
    is_creator: bool,
}

// SharedMemory is Send — the underlying OS handle can be used from any thread.
// It is *not* Sync because concurrent writes to the raw data region are
// unsynchronised (matching the C++ library semantics).
unsafe impl Send for SharedMemory {}

/// Errors returned when creating or opening a segment fails.
#[derive(Debug, Clone)]
pub struct Error {
    kind: ErrorKind,
}

#[derive(Debug, Clone, Copy)]
enum ErrorKind {
    InvalidName,
    CreateFailed,
    OpenFailed,
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match self.kind {
            ErrorKind::InvalidName => write!(f, "shared memory name contains a null byte"),
            ErrorKind::CreateFailed => write!(f, "failed to create shared memory segment"),
            ErrorKind::OpenFailed => write!(f, "failed to open shared memory segment"),
        }
    }
}

impl std::error::Error for Error {}

impl SharedMemory {
    /// Create a new shared memory segment.
    ///
    /// `name` is an OS-wide identifier (alpha-numeric recommended).
    /// `size` is the capacity in bytes (up to 4 GiB).
    /// `persistent` controls whether the segment outlives the process.
    pub fn create(name: &str, size: usize, persistent: bool) -> Result<Self, Error> {
        let c_name = CString::new(name).map_err(|_| Error { kind: ErrorKind::InvalidName })?;
        let ptr = unsafe { ffi::lsm_create(c_name.as_ptr(), size, persistent as i32) };
        NonNull::new(ptr)
            .map(|inner| SharedMemory { inner, size, is_creator: true })
            .ok_or(Error { kind: ErrorKind::CreateFailed })
    }

    /// Open an existing shared memory segment for reading.
    pub fn open(name: &str, size: usize, persistent: bool) -> Result<Self, Error> {
        let c_name = CString::new(name).map_err(|_| Error { kind: ErrorKind::InvalidName })?;
        let ptr = unsafe { ffi::lsm_open(c_name.as_ptr(), size, persistent as i32) };
        NonNull::new(ptr)
            .map(|inner| SharedMemory { inner, size, is_creator: false })
            .ok_or(Error { kind: ErrorKind::OpenFailed })
    }

    /// Returns the size of the mapped region in bytes.
    pub fn size(&self) -> usize {
        self.size
    }

    /// Returns a read-only slice over the mapped region.
    pub fn as_slice(&self) -> &[u8] {
        unsafe {
            let ptr = ffi::lsm_data(self.inner.as_ptr()) as *const u8;
            std::slice::from_raw_parts(ptr, self.size)
        }
    }

    /// Returns a mutable slice over the mapped region.
    pub fn as_mut_slice(&self) -> &mut [u8] {
        unsafe {
            let ptr = ffi::lsm_data(self.inner.as_ptr()) as *mut u8;
            std::slice::from_raw_parts_mut(ptr, self.size)
        }
    }

    /// Returns a raw pointer to the mapped data.
    pub fn as_ptr(&self) -> *mut u8 {
        unsafe { ffi::lsm_data(self.inner.as_ptr()) as *mut u8 }
    }

    /// Unlink the shared memory segment from the OS.
    ///
    /// After this call the name is freed and the backing storage will be
    /// reclaimed once all handles are closed.
    pub fn destroy(&self) {
        unsafe { ffi::lsm_destroy(self.inner.as_ptr()) }
    }
}

impl Drop for SharedMemory {
    fn drop(&mut self) {
        unsafe {
            ffi::lsm_close(self.inner.as_ptr());
            if self.is_creator {
                ffi::lsm_destroy(self.inner.as_ptr());
            }
            ffi::lsm_free(self.inner.as_ptr());
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn roundtrip() {
        let msg = b"Hello from Rust!";
        let writer = SharedMemory::create("rust_test_roundtrip", 256, true)
            .expect("create failed");
        writer.as_mut_slice()[..msg.len()].copy_from_slice(msg);

        let reader = SharedMemory::open("rust_test_roundtrip", 256, true)
            .expect("open failed");
        assert_eq!(&reader.as_slice()[..msg.len()], msg);
    }

    #[test]
    fn size_matches() {
        let mem = SharedMemory::create("rust_test_size", 512, true)
            .expect("create failed");
        assert_eq!(mem.size(), 512);
    }

    #[test]
    fn invalid_name_rejected() {
        assert!(SharedMemory::create("bad\0name", 64, false).is_err());
    }
}
