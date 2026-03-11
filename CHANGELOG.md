# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [2.0.0] - 2026-03-11

### Added
- Stream metadata now includes revision/ack tracking to prevent missed unread-update signaling when writers outpace reader acknowledgements
- Stream writer serialization lock and reader snapshot consistency checks to avoid torn reads under concurrent writers
- Queue producer serialization lock to prevent concurrent producer slot/index races
- Regression test coverage for Issue #3 behavior and concurrent writer/producer coherence

### Changed
- Stream wire layout metadata changed from `|flags|size|data|` to an extended metadata header (flags + revision + ack + size + lock + data)
- Queue metadata header expanded to include producer lock state

### Fixed
- `hasNewData()` no longer drops unread updates when multiple writes occur before `markAsRead()`
- Concurrent stream writers no longer produce mixed/torn payload snapshots in stress tests
- Concurrent queue producers no longer corrupt message prefixes or race write index updates in stress tests

### Performance
- Expected performance impact: slight to moderate throughput drop in write-heavy workloads due to writer/producer locking and reader snapshot retry checks
- Typical impact is low for single-writer/single-producer use, and higher under heavy contention where correctness is now prioritized

### Breaking Changes
- This is a wire-layout breaking change for stream and queue metadata
- Existing processes built against the old in-memory format must not interoperate with this new build until all participants are updated together

## [1.10.0] - 2026-03-11

### Added
- Makefile with `build`, `test`, `clean`, `examples` and `setup` targets wrapping CMake
- Re-verified current macOS aarch64 (Apple Silicon) support on Sequoia 15.7.3
- New test case on `Memory` accessors, `getWriteFlags` logic, `std::span` write overloads, segment overwrite, queue capacity=1 edge case, `maxMessageSize` boundary and concurrent writing; also added docs to understand the tests better
- `.vscode/c_cpp_properties.json` for C++20 IntelliSense on macOS (arm64/aarch64)
- No-flake stability test: re-runs the full suite 1000 times as subprocesses to catch timing-dependent failures (cross-platform, skips itself via `LSM_NOFLAKE` env var)
- Added `macos-latest` to GitHub Actions CI matrix with `LSM_NOFLAKE=1` to skip the 1000-iteration flake test in CI
- `example/` directory with three runnable examples: stream transfer (C++), message queue (C++), and raw shared memory (C via `lsm_c` wrapper)
- `make examples` target to build and run all examples
- Pure C wrapper (`lsm_c.h` / `lsm_c.cpp`) exposing `Memory` as opaque-handle C functions
- Rust FFI bindings (`ffi/rust/`) with safe `SharedMemory` wrapper, unit tests, and runnable example
- `ffi/rust/Makefile` with `setup`, `build`, `test`, `example`, and `clean` targets
- Zig FFI bindings (`ffi/zig/`) with idiomatic `SharedMemory` wrapper, unit tests, and runnable example (Zig 0.15+)
- `ffi/zig/Makefile` with `setup`, `build`, `test`, `example`, and `clean` targets
- Go FFI bindings (`ffi/go/`) via cgo with `SharedMemory` wrapper, unit tests, and runnable example
- `ffi/go/Makefile` with `setup`, `build`, `test`, `example`, and `clean` targets

### Changed
- Improved README: added supported platforms table, building instructions, tightened examples and documentation
- Added more detailed documentation for `SharedMemoryQueue` and Windows persistency features in README
- Added Makefile to simplify setup, build and test processes for developers
- README: added screenshot, pure C example with `lsm_c` wrapper

## [1.9.0] - 2026-02-13

### Added
- **Linux**: Default permissions set to 0777.


## [1.8.0] - 2026-02-06

### Added
- **Windows**: ACL inheritance for shared file, clean close/reopen when applying settings.

## [1.7.0] - 2026-02-06

### Added
- **SharedMemoryQueue**: New FIFO message queue functionality for inter-process communication
  - Thread-safe enqueue/dequeue operations using atomic counters
  - Configurable capacity and maximum message size
  - `peek()` method to inspect messages without removing them
  - Status methods: `isEmpty()`, `isFull()`, `size()`, `capacity()`
  - Supports single producer/single consumer and single producer/multiple consumers patterns
- Windows persistency support for shared memory segments
  - Persistent file-backed shared memory on Windows using `%PROGRAMDATA%/shared_memory/` directory
  - Automatic directory creation and permission management on Windows
  - Configurable persistence with `persist` parameter in constructor

### Changed
- Updated C++20 requirements with `std::atomic` support for queue operations
- Improved memory layout with atomic operations for queue counters
- Enhanced Windows implementation with better file handle management

### Fixed
- Memory leak prevention in queue implementation
- Proper cleanup of shared memory on all platforms

## Previous Versions

Prior to version 1.7.0, the library supported:
- Basic shared memory creation and access on Windows, Linux, and macOS
- Stream-based data transfer with `SharedMemoryWriteStream` and `SharedMemoryReadStream`
- Support for `std::string`, `float*`, and `double*` array types
- Change detection with flag bit flipping
- Single value access via `.data()[index]` API

