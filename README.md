# libsharedmemory

A lightweight, header-only C++20 library for inter-process communication via shared memory. Transfer data between isolated OS processes - or between modules written in different programming languages - with a simple, cross-platform API.

**Key capabilities:**
- Stream-based read/write transfer (`std::string`, `float*`, `double*`, scalars)
- FIFO message queue (`SharedMemoryQueue`) with atomic operations
- Optional persistence for shared memory segments
- Change detection via flag bit flipping

## Supported Platforms

| Platform | Architecture |
|---|---|
| Windows | x86_64 |
| Linux | x86_64, aarch64 |
| macOS | x86_64, aarch64 (Apple Silicon) |

## Building

Requires CMake 3.12+ and a C++20 compatible compiler.

```sh
make build    # Configure and build (Release)
make test     # Build and run all tests
make clean    # Remove build artifacts
```

Or use CMake directly:

```sh
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

## Examples

### Stream-based Transfer

```cpp
std::string data = R"({ "status": "connected", "protocol": "shm" })";

// Create writer and reader (name is OS-wide, size in bytes, up to 4 GiB)
SharedMemoryWriteStream writer{"myChannel", /*size*/ 65535, /*persistent*/ true};
SharedMemoryReadStream reader{"myChannel", /*size*/ 65535, /*persistent*/ true};

writer.write(data);

// Read from the same or another process, thread, or application
std::string result = reader.readString();
```

### Message Queue (C++20)

```cpp
SharedMemoryQueue writer{"queue", /*capacity*/ 10, /*maxMessageSize*/ 256, /*persistent*/ true, /*isWriter*/ true};
SharedMemoryQueue reader{"queue", /*capacity*/ 10, /*maxMessageSize*/ 256, /*persistent*/ true, /*isWriter*/ false};

writer.enqueue("First message");
writer.enqueue("Second message");

std::string msg;
if (reader.dequeue(msg)) {
    std::cout << "Received: " << msg << std::endl;
}

// Peek without removing
if (reader.peek(msg)) {
    std::cout << "Next: " << msg << std::endl;
}

std::cout << "Size: " << reader.size() << ", Empty: " << reader.isEmpty() << std::endl;
```

## Features

### Stream-based Transfer
- `std::string` (UTF-8 compatible), `float*`, `double*` arrays
- Single value access via `.data()[index]` for all C/C++ scalar types
- Change detection with automatic flag bit flipping

### Message Queue
- Thread-safe enqueue/dequeue using atomic counters
- Configurable capacity and maximum message size
- Peek functionality to inspect without consuming
- Supports single-producer/single-consumer and single-producer/multiple-consumer patterns

## Installation

Copy `include/libsharedmemory/libsharedmemory.hpp` into your project's include path - it's a single header.

Alternatively, use `npm` for dependency management:

```sh
npm init                          # If not already initialized
npm install cpp_libsharedmemory   # Install the library
```

Then add `node_modules/cpp_libsharedmemory/include/libsharedmemory` to your include path. Use `npm upgrade` to pull updates and `npm audit` to check for security advisories.

## Memory Layout

Each named shared memory segment includes 5 bytes of metadata:

| Field | Type | Size | Description |
|---|---|---|---|
| `flags` | `char` | 1 byte | Bitmask: change indicator + data type |
| `size` | `int` | 4 bytes | Buffer size in bytes |

Binary layout: `|flags|size|data|`

```c
enum DataType {
  kMemoryChanged = 1,
  kMemoryTypeString = 2,
  kMemoryTypeFloat = 4,
  kMemoryTypeDouble = 8,
};
```

`kMemoryChanged` flips odd/even to signal data changes, allowing continuous readers to detect every update.

## Limits and Frequently Asked Questions

### Can I use this for cross-platform network communication?

No. **Endianness** is not handled. This is fine for local shared memory but requires attention if copying buffers to a network protocol.

### What about cross compiler compatibility?

**Cross-compiler** behavior for the binary memory layout is undefined. The library is designed for C++20 compliant compilers on the same platform. For cross-compiler or cross-language interoperability, you must ensure consistent data type sizes, alignment, and endianness.

### Can I use this with multiple writers?

Maybe for slow writers, **if you are lucky**.
**Concurrent writers**(`SharedMemoryWriteStream`) are currently not safely supported. `write()` performs 3 non-atomic `memcpy` calls (flags, size, data). Two threads writing to the same segment can interleave these operations, producing torn reads with mixed content or incorrect sizes. Use a single writer per segment or add external synchronization.

### Are multiple producers supported for `SharedMemoryQueue`?

Not yet. `enqueue()` uses a non-atomic read-modify-write on the write index. Two threads calling `enqueue()` on the same queue will read the same slot index, overwrite each other's data, and advance the index only once - causing **message corruption** in testing (up to 45% on macOS 15.7, aarch64, Macbook Air M4, 1000 messages, 2 producers). Use a single producer per queue or add a mutex around `enqueue()`.

## Roadmap

1. Non-blocking `onChange(lambda)` handler on the read stream
2. Lock-free multi-producer support for `SharedMemoryQueue`

## License

MIT - see [LICENSE](LICENSE).
