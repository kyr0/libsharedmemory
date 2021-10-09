
#ifndef INCLUDE_KYR0_LIBSHAREDMEMORY_HPP_
#define INCLUDE_KYR0_LIBSHAREDMEMORY_HPP_

#define KYR0_LIBSHAREDMEMORY_VERSION_MAJOR 1
#define KYR0_LIBSHAREDMEMORY_VERSION_MINOR 0
#define KYR0_LIBSHAREDMEMORY_VERSION_PATCH 0
#endif

#include <cstdint>
#include <cstring>
#include <string>
#include <cstddef> // nullptr_t, ptrdiff_t, size_t

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
  kMemoryFloat32Vector = 4,
  kMemoryUInt8Vector = 8,
};

class Memory {
public:
    // path should only contain alpha-numeric characters, and is normalized
    // on linux/macOS.
    explicit Memory(std::string path, size_t size, bool persist);

    // create a shared memory area and open it for writing
    inline Error create() { return createOrOpen(true); };

    // open an existing shared memory for reading
    inline Error open() { return createOrOpen(false); };

    inline size_t size() { return _size; };

    inline const std::string &path() { return _path; }

    inline uint8_t *data() { return _data; }

    void destroy();

    ~Memory();

private:
    Error createOrOpen(bool create);

    std::string _path;
    uint8_t *_data = nullptr;
    size_t _size = 0;
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

Memory::Memory(std::string path, size_t size, bool persist) : _path(path), _size(size), _persist(persist) {};

Error Memory::createOrOpen(bool create) {
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
        _handle = OpenFileMappingA(FILE_MAP_READ,  // read access
                                    FALSE,          // do not inherit the name
                                    _path.c_str()   // name of mapping object
        );

        if (!_handle) {
            return kErrorOpeningFailed;
        }
    }

    DWORD access = create ? FILE_MAP_ALL_ACCESS : FILE_MAP_READ;
    _data = static_cast<uint8_t *>(MapViewOfFile(_handle, access, 0, 0, _size));

    if (!_data) {
        return kErrorMappingFailed;
    }
    return kOK;
}

void Memory::destroy() {
    if (_data) {
        UnmapViewOfFile(_data);
        _data = nullptr;
    }
    CloseHandle(_handle);
}

Memory::~Memory() {
    destroy()
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

inline Memory::Memory(std::string path, size_t size, bool persist) : _size(size), _persist(persist) {
    _path = "/" + path;
};

inline Error Memory::createOrOpen(bool create) {
    if (create) {
        // shm segments persist across runs, and macOS will refuse
        // to ftruncate an existing shm segment, so to be on the safe
        // side, we unlink it beforehand.
        int ret = shm_unlink(_path.c_str());
        if (ret < 0) {
            if (errno != ENOENT) {
                return kErrorCreationFailed;
            }
        }
    }

    int flags = create ? (O_CREAT | O_RDWR) : O_RDONLY;

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

    int prot = create ? (PROT_READ | PROT_WRITE) : PROT_READ;

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

    _data = static_cast<uint8_t *>(memory);

    if (!_data) {
        return kErrorMappingFailed;
    }
    return kOK;
}

inline void Memory::destroy() {
    munmap(_data, _size);
    close(_fd);
    shm_unlink(_path.c_str());
}

inline Memory::~Memory() {
    if (!_persist) {
        destroy();
    }
}

#endif // defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION) || defined(__ANDROID__)

class SharedMemoryReadStream {
public:

    explicit SharedMemoryReadStream(std::string name, uint32_t bufferSize, bool isPersistent): 
        _memory(name, bufferSize, isPersistent)/*, _isInOddWriteMode(false)*/ {

        if (_memory.open() != kOK) {
            throw "Shared memory segment could not be opened.";
        }
    }

    inline std::string read() {
        unsigned char* memory = _memory.data();

        uint32_t size;
        std::memcpy(&size, &memory[1], 4 /*uint32 takes 4 byte*/);

        // 3) deserialize the buffer vector data
        std::string data(reinterpret_cast<const char*>(&memory[5]), size);
        return data;
    }

private:
    Memory _memory;
    //bool _isInOddWriteMode;
};

class SharedMemoryWriteStream {
public:

    explicit SharedMemoryWriteStream(std::string name, uint32_t bufferSize, bool isPersistent): 
        _memory(name, bufferSize, isPersistent), _isInOddWriteMode(false) {

        if (_memory.create() != kOK) {
            throw "Shared memory segment could not be created.";
        }
    }

    // https://stackoverflow.com/questions/18591924/how-to-use-bitmask
    inline uint32_t getWriteFlags(uint8_t type) {
        // flip state
        _isInOddWriteMode = !_isInOddWriteMode;
        unsigned char flags = type;

        if (_isInOddWriteMode) {
            // enable flag, leave rest untouched
            flags ^= DataType::kMemoryChanged;
        } else {
            // disable flag, leave rest untouched
            flags &= ~DataType::kMemoryChanged;
        }
        return flags;
    }

    inline void write(std::string dataString) {
        unsigned char* memory = _memory.data();

        // 1) copy change flag into buffer for change detection
        memory[0] = getWriteFlags(DataType::kMemoryTypeString);

        // 2) copy buffer size into buffer (meta data for deserializing)
        const char *stringDataVector = dataString.data();
        uint32_t bufferSize = dataString.size();
        std::memcpy(&memory[1], &bufferSize, sizeof(bufferSize) /* should be always 4 */);

        // 3) copy data vector into memory buffer
        std::memcpy(&memory[5 /* 1b status; 4b buffer size */], stringDataVector, bufferSize);
    }

    inline void destroy() {
        _memory.destroy();
    }

private:
    Memory _memory;
    bool _isInOddWriteMode;
};


}; // namespace lsm

