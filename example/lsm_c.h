// Minimal C wrapper around lsm::Memory.
//
// This is intentionally thin - it exposes only the parts of the C++ API
// that are meaningful from plain C (raw byte read/write via void*).
// Defined as a header-only implementation to keep the example self-contained.

#ifndef LSM_C_H
#define LSM_C_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque handle
typedef struct lsm_memory lsm_memory;

// Create a new shared memory segment. Returns NULL on failure.
lsm_memory* lsm_create(const char* name, size_t size, int persistent);

// Open an existing shared memory segment. Returns NULL on failure.
lsm_memory* lsm_open(const char* name, size_t size, int persistent);

// Get a pointer to the mapped data region.
void* lsm_data(lsm_memory* mem);

// Get the size of the mapped region in bytes.
size_t lsm_size(lsm_memory* mem);

// Close the handle (unmaps memory).
void lsm_close(lsm_memory* mem);

// Destroy the shared memory segment (unlinks the name from the OS).
void lsm_destroy(lsm_memory* mem);

// Free the handle allocated by lsm_create / lsm_open.
void lsm_free(lsm_memory* mem);

#ifdef __cplusplus
}
#endif

#endif // LSM_C_H
