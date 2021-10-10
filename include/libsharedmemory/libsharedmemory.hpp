
#ifndef INCLUDE_LIBSHAREDMEMORY_HPP_
#define INCLUDE_LIBSHAREDMEMORY_HPP_

#define LIBSHAREDMEMORY_VERSION_MAJOR 0
#define LIBSHAREDMEMORY_VERSION_MINOR 0
#define LIBSHAREDMEMORY_VERSION_PATCH 3

#include <chrono>
#include <cstdint>
#include <cstring>
#include <string>
#include <mutex>
#include <thread>
#include <iostream>
#include <cstddef> // nullptr_t, ptrdiff_t, std::size_t

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#undef WIN32_LEAN_AND_MEAN
#endif

namespace lsm {

enum Error {
  kOK = 0,
  kErrorCreationFailed = 100,
  kErrorMappingFailed = 110,
  kErrorOpeningFailed = 120,
};

enum DataType {
  kMemoryChanged = 1,
  kMemoryTypeString = 2,
};

// byte sizes of memory layout
const size_t bufferSizeSize = 4;
const size_t flagSize = 1;

class Memory {
public:
    // path should only contain alpha-numeric characters, and is normalized
    // on linux/macOS.
    explicit Memory(std::string path, std::size_t size, bool persist);

    // create a shared memory area and open it for writing
    inline Error create() { return createOrOpen(true); };

    // open an existing shared memory for reading
    inline Error open() { return createOrOpen(false); };

    inline std::size_t size() { return _size; };

    inline const std::string &path() { return _path; }

    inline unsigned char *data() { return _data; }

    void destroy();

    ~Memory();

private:
    Error createOrOpen(bool create);

    std::string _path;
    unsigned char *_data = nullptr;
    std::size_t _size = 0;
    bool _persist = true;
#if defined(_WIN32)
    HANDLE _handle;
#else
    int _fd = -1;
#endif
};

// Windows shared memory implementation
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

#include <io.h>  // CreateFileMappingA, OpenFileMappingA, etc.

Memory::Memory(const std::string path, const std::size_t size, const bool persist) : _path(path), _size(size), _persist(persist) {};

Error Memory::createOrOpen(const bool create) {
    if (create) {
        DWORD size_high_order = 0;
        DWORD size_low_order = static_cast<DWORD>(size_);

        _handle = CreateFileMappingA(INVALID_HANDLE_VALUE,  // use paging file
                                        NULL,                  // default security
                                        PAGE_READWRITE,        // read/write access
                                        size_high_order, size_low_order,
                                        _path.c_str()  // name of mapping object
        );

        if (!_handle) {
            return kErrorCreationFailed;
        }
    } else {
      _handle = OpenFileMappingA(FILE_MAP_READ, // read access
                                 FALSE,         // do not inherit the name
                                 _path.c_str()  // name of mapping object
      );

      // TODO: Windows has no default support for shared memory persistence
      // see: destroy() to implement that

        if (!_handle) {
            return kErrorOpeningFailed;
        }
    }

    // TODO: might want to use GetWriteWatch to get called whenever
    // the memory section changes
    // https://docs.microsoft.com/de-de/windows/win32/api/memoryapi/nf-memoryapi-getwritewatch?redirectedfrom=MSDN

    const DWORD access = create ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
    _data = static_cast<unsigned char *>(MapViewOfFile(_handle, access, 0, 0, _size));

    if (!_data) {
        return kErrorMappingFailed;
    }
    return kOK;
}

void Memory::destroy() {

  // TODO: Windows needs priviledges to define a shared memory (file mapping)
  // OBJ_PERMANENT; furthermore, ZwCreateSection would need to be used.
  // Instead of doing this; saving a file here (by name, temp dir)
  // and reading memory from file in createOrOpen seems more suitable.
  // Especially, because files can be removed on reboot using:
  // MoveFileEx() with the MOVEFILE_DELAY_UNTIL_REBOOT flag and lpNewFileName
  // set to NULL.
}

Memory::~Memory() {
    if (_data) {
        UnmapViewOfFile(_data);
        _data = nullptr;
    }
    CloseHandle(_handle);
    if (!_persist) {
      destroy();
    }
}
#endif // defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION) || defined(__ANDROID__)

#include <fcntl.h>     // for O_* constants
#include <sys/mman.h>  // mmap, munmap
#include <sys/stat.h>  // for mode constants
#include <unistd.h>    // unlink

#if defined(__APPLE__)

#include <errno.h>

#endif // __APPLE__

#include <stdexcept>

inline Memory::Memory(const std::string path, const std::size_t size, const bool persist) : _size(size), _persist(persist) {
    _path = "/" + path;
};

inline Error Memory::createOrOpen(const bool create) {
    if (create) {
        // shm segments persist across runs, and macOS will refuse
        // to ftruncate an existing shm segment, so to be on the safe
        // side, we unlink it beforehand.
        const int ret = shm_unlink(_path.c_str());
        if (ret < 0) {
            if (errno != ENOENT) {
                return kErrorCreationFailed;
            }
        }
    }

    const int flags = create ? (O_CREAT | O_RDWR) : O_RDONLY;

    _fd = shm_open(_path.c_str(), flags, 0755);
    if (_fd < 0) {
        if (create) {
            return kErrorCreationFailed;
        } else {
            return kErrorOpeningFailed;
        }
    }

    if (create) {
        // this is the only way to specify the size of a
        // newly-created POSIX shared memory object
        int ret = ftruncate(_fd, _size);
        if (ret != 0) {
            return kErrorCreationFailed;
        }
    }

    const int prot = create ? (PROT_READ | PROT_WRITE) : PROT_READ;

    void *memory = mmap(nullptr,    // addr
                        _size,      // length
                        prot,       // prot
                        MAP_SHARED, // flags
                        _fd,        // fd
                        0           // offset
    );

    if (memory == MAP_FAILED) {
        return kErrorMappingFailed;
    }

    _data = static_cast<unsigned char *>(memory);

    if (!_data) {
        return kErrorMappingFailed;
    }
    return kOK;
}

inline void Memory::destroy() {
    shm_unlink(_path.c_str());
}

inline Memory::~Memory() {
    munmap(_data, _size);
    close(_fd);
    if (!_persist) {
        destroy();
    }
}

#endif // defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION) || defined(__ANDROID__)

class SharedMemoryReadStream {
public:

    explicit SharedMemoryReadStream(const std::string name, const std::size_t bufferSize, const bool isPersistent): 
        _memory(name, bufferSize, isPersistent)/*, _isInOddWriteMode(false)*/ {

        if (_memory.open() != kOK) {
            throw "Shared memory segment could not be opened.";
        }
    }

    inline std::string read() {
        unsigned char* memory = _memory.data();

        std::size_t size = 0;

        // copy buffer size
        std::memcpy(&size, &memory[flagSize], bufferSizeSize);

        // create a string that copies the data from memory
        // location while re-interpreting unsinged char* to const char*
        std::string data = std::string(
            reinterpret_cast<const char *>(&memory[flagSize + bufferSizeSize]),
            size);
        return data;
    }

    /*

    void onChange(void (*cb)(std::string&)) {
        std::thread t;

      t = std::thread([&] {
        unsigned char _flags;
        unsigned char flags = _memory.data()[0];

        std::string &data = read();

        if (cb && data[0]) {
          cb(data);
        }

        while (true) {

          //std::this_thread::sleep_for(std::chrono::microseconds(1));

          if (_memory.data() && _memory.data()[0]) {
            _flags = _memory.data()[0];

            if (((_flags & (kMemoryChanged)) == (kMemoryChanged)) !=
                ((flags & (kMemoryChanged)) == (kMemoryChanged))) {

                if (cb) {
                  std::string data = read();

                  if (data[0]) {
                    cb(data);
                }
                }
            }
          }
        }
      });
        t.detach();
    }
    */

private:
    Memory _memory;
};

class SharedMemoryWriteStream {
public:

    explicit SharedMemoryWriteStream(const std::string name, const std::size_t bufferSize, const bool isPersistent): 
        _memory(name, bufferSize, isPersistent) {

        if (_memory.create() != kOK) {
            throw "Shared memory segment could not be created.";
        }
    }

    // https://stackoverflow.com/questions/18591924/how-to-use-bitmask
    inline unsigned char getWriteFlags(const unsigned char type,
                                       const unsigned char currentFlags) {
        unsigned char flags = type;

        if ((currentFlags & (kMemoryChanged)) == kMemoryChanged) {
            // disable flag, leave rest untouched
            flags &= ~kMemoryChanged;
        } else {
            // enable flag, leave rest untouched
            flags ^= kMemoryChanged;
        }
        return flags;
    }

    inline void write(const std::string& dataString) {
        unsigned char* memory = _memory.data();

        // 1) copy change flag into buffer for change detection
        memory[0] = getWriteFlags(kMemoryTypeString, memory[0]);

        // 2) copy buffer size into buffer (meta data for deserializing)
        const char *stringData = dataString.data();
        const std::size_t bufferSize = dataString.size();
        std::memcpy(&memory[flagSize], &bufferSize, bufferSizeSize);

        // 3) copy stringData into memory buffer
        std::memcpy(&memory[flagSize + bufferSizeSize], stringData, bufferSize);
    }

    inline void destroy() {
        _memory.destroy();
    }

private:
    Memory _memory;
};


}; // namespace lsm

#endif // INCLUDE_LIBSHAREDMEMORY_HPP_