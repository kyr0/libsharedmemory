//! Raw FFI bindings to the `lsm_c` C wrapper.
#![allow(dead_code)]

use std::os::raw::{c_char, c_int, c_void};

/// Opaque handle returned by the C wrapper.
#[repr(C)]
pub struct lsm_memory {
    _opaque: [u8; 0],
}

extern "C" {
    pub fn lsm_create(name: *const c_char, size: usize, persistent: c_int) -> *mut lsm_memory;
    pub fn lsm_open(name: *const c_char, size: usize, persistent: c_int) -> *mut lsm_memory;
    pub fn lsm_data(mem: *mut lsm_memory) -> *mut c_void;
    pub fn lsm_size(mem: *mut lsm_memory) -> usize;
    pub fn lsm_close(mem: *mut lsm_memory);
    pub fn lsm_destroy(mem: *mut lsm_memory);
    pub fn lsm_free(mem: *mut lsm_memory);
}
