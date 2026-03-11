// Package lsm provides Go bindings for libsharedmemory via cgo.
//
// It wraps the C API (lsm_c.h) which in turn wraps the header-only
// C++20 library, allowing Go programs to create and access shared
// memory segments that any C++ (or Rust, Zig, C) process can read.
package lsm

/*
#cgo LDFLAGS: -L${SRCDIR}/lib -llsm_c -lstdc++
#cgo CFLAGS: -I${SRCDIR}/../../example
#include "lsm_c.h"
#include <stdlib.h>
#include <string.h>
*/
import "C"

import (
	"errors"
	"unsafe"
)

// SharedMemory is a handle to a shared memory segment.
type SharedMemory struct {
	handle    *C.lsm_memory
	size      int
	isCreator bool
}

var (
	ErrCreateFailed = errors.New("lsm: failed to create shared memory segment")
	ErrOpenFailed   = errors.New("lsm: failed to open shared memory segment")
)

// Create creates a new shared memory segment.
// name is an OS-wide identifier, size is capacity in bytes,
// persistent controls whether the segment outlives the process.
func Create(name string, size int, persistent bool) (*SharedMemory, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	p := 0
	if persistent {
		p = 1
	}

	handle := C.lsm_create(cName, C.size_t(size), C.int(p))
	if handle == nil {
		return nil, ErrCreateFailed
	}
	return &SharedMemory{handle: handle, size: size, isCreator: true}, nil
}

// Open opens an existing shared memory segment for reading.
func Open(name string, size int, persistent bool) (*SharedMemory, error) {
	cName := C.CString(name)
	defer C.free(unsafe.Pointer(cName))

	p := 0
	if persistent {
		p = 1
	}

	handle := C.lsm_open(cName, C.size_t(size), C.int(p))
	if handle == nil {
		return nil, ErrOpenFailed
	}
	return &SharedMemory{handle: handle, size: size, isCreator: false}, nil
}

// Size returns the size of the mapped region in bytes.
func (m *SharedMemory) Size() int {
	return m.size
}

// Data returns a byte slice backed by the mapped region.
// The slice is valid until Close is called.
func (m *SharedMemory) Data() []byte {
	ptr := C.lsm_data(m.handle)
	return unsafe.Slice((*byte)(ptr), m.size)
}

// Write copies src into the shared memory at offset 0.
func (m *SharedMemory) Write(src []byte) {
	dst := C.lsm_data(m.handle)
	C.memcpy(dst, unsafe.Pointer(&src[0]), C.size_t(len(src)))
}

// Destroy unlinks the segment from the OS.
func (m *SharedMemory) Destroy() {
	C.lsm_destroy(m.handle)
}

// Close unmaps the memory and frees the handle.
// If this handle was the creator, the segment is also destroyed.
func (m *SharedMemory) Close() {
	C.lsm_close(m.handle)
	if m.isCreator {
		C.lsm_destroy(m.handle)
	}
	C.lsm_free(m.handle)
}
