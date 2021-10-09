# `libsharedmemory`

`libsharedmemory` is a small C++11 header-only library for using shared memory on Windows, Linux and macOS. It makes it easy to transfer data between isolated processes and threads. It also helps inter-connecting . It allows for simple read/write data transfer of `uint8_t*` / `unsigned char*` and `std::string`.

<img src="screenshot.png" width="350px" />

## Example

```cpp

std::string dataToTransfer = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

// the name of the shared memory is os-wide announced
// the size is in bytes and must be big enough to handle the data (up to 4GiB)
// if persistency is disabled, the shared memory segment will
// be garbage collected when the process that wrote it is killed
SharedMemoryWriteStream write$ {/*name*/ "jsonPipe", /*size*/ 65535, /*persistent*/ true};
SharedMemoryReadStream read$ {/*name*/ "jsonPipe", /*size*/ 65535, /*persistent*/ true};

// writing the string to the shared memory
write$.write(dataToTransfer);

// reading the string from shared memory
// you can run this in another process, thread,
// even in another app written in another programming language
std::string dataString = read$.read();

std::cout << "UTF8 string written and read" << dataString << std::endl;
```

## Limits

`libsharedmemory` does only support the following datatypes:
- UTF8 strings (`std::string`)
- byte buffers, small integers (`uint8_t*`, `unsigned char*`)

There is no explicit handling of endinanness. Memory interpretation 
issues may happen when transferring the memory between different machines/vm.

Although the binary memory layout should give you no headache
when compiling/linking using different compilers, 
the behavior is undefined.

## Memory layout

When writing data into a named shared memory segment, `libsharedmemory`
does write 5 bytes of meta information:

- `flags` (`uint8_t`) is a bitmask that indicates data change (via an odd/even bit flip) as well as the data type transferred (1 byte)
- `size` (`uint32_t`) indicates the buffer size in bytes (4 bytes)

Therefore the binary memory layout is:
|flags|size|data|

The following flags are defined:
```c
enum DataType {
  kMemoryChanged = 1,
  kMemoryTypeString = 2,
};
```

## Build

This project is meant to be built with `cmake` and `clang`.
However, it _should_ also build with MSVC and GCC.

```sh
./build.sh
```

## Test

Test executables are built automatically and can be executed
to verify the correct function of the implementation on your machine:

```sh
./test.sh
```

## License

`libsharedmemory` is released under the MIT license, see the `LICENSE` file.

## Roadmap

1) Support for `float32*`, `float64*`, vector data types (without the vector container, `vec.data()`)
2) Multi-threaded non-blocking `onChange( lambda fn )` data change handler on the read stream