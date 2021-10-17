
#ifndef INCLUDE_LIBSHAREDMEMORY_HPP_
#define INCLUDE_LIBSHAREDMEMORY_HPP_

#include <ostream>
#define LIBSHAREDMEMORY_VERSION_MAJOR 0
#define LIBSHAREDMEMORY_VERSION_MINOR 0
#define LIBSHAREDMEMORY_VERSION_PATCH 9

#include <cstdint>
#include <cstring>
#include <string>
#include <unistd.h>
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
  kMemoryTypeFloat = 4,
  kMemoryTypeDouble = 8,
};

// byte sizes of memory layout
const size_t bufferSizeSize = 4; // size_t takes 4 bytes
const size_t sizeOfOneFloat = 4; // float takes 4 bytes
const size_t sizeOfOneChar = 1; // char takes 1 byte
const size_t sizeOfOneDouble = 8; // double takes 8 bytes
const size_t flagSize = 1; // char takes 1 byte

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

    inline void *data() { return _data; }

    void destroy();

    void close();

    ~Memory();

private:
    Error createOrOpen(bool create);

    std::string _path;
    void *_data = nullptr;
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
    _data = MapViewOfFile(_handle, access, 0, 0, _size);

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

void Memory::close() {
    if (_data) {
        UnmapViewOfFile(_data);
        _data = nullptr;
    }
    CloseHandle(_handle);
}

Memory::~Memory() {
    close();
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

    _data = mmap(nullptr,    // addr
                 _size,      // length
                 prot,       // prot
                 MAP_SHARED, // flags
                 _fd,        // fd
                 0           // offset
    );

    if (_data == MAP_FAILED) {
        return kErrorMappingFailed;
    }

    if (!_data) {
        return kErrorMappingFailed;
    }
    return kOK;
}

inline void Memory::destroy() { shm_unlink(_path.c_str()); }

inline void Memory::close() {
  munmap(_data, _size);
  ::close(_fd);
}

inline Memory::~Memory() {
    close();
    if (!_persist) {
        destroy();
    }
}

#endif // defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION) || defined(__ANDROID__)

class SharedMemoryReadStream {
public:

    SharedMemoryReadStream(const std::string name, const std::size_t bufferSize, const bool isPersistent): 
        _memory(name, bufferSize, isPersistent) {

        if (_memory.open() != kOK) {
            throw "Shared memory segment could not be opened.";
        }
    }

    inline char readFlags() {
      char* memory = (char*) _memory.data();
      return memory[0];
    }

    inline void close() { _memory.close(); }


    inline size_t readSize(char dataType) {
        void *memory = _memory.data();
        std::size_t size = 0;

        // TODO(kyr0): should be clarified why we need to use size_t there
        // for the size to be received correctly, but in float, we need int
        // Might be prone to undefined behaviour; should be tested
        // with various compilers; otherwise use memcpy() for the size
        // and align the memory with one cast.

        if (dataType & kMemoryTypeDouble) {
            size_t *intMemory = (size_t *)memory;
            // copy size data to size variable
            std::memcpy(&size, &intMemory[flagSize], bufferSizeSize);
        }

        if (dataType & kMemoryTypeFloat) {
            int* intMemory = (int*) memory; 
            // copy size data to size variable
            std::memcpy(&size, &intMemory[flagSize], bufferSizeSize);
        }

        if (dataType & kMemoryTypeString) {
            char* charMemory = (char*) memory; 
            // copy size data to size variable
            std::memcpy(&size, &charMemory[flagSize], bufferSizeSize);
        }
        return size;
    }

    inline size_t readLength(char dataType) {
      size_t size = readSize(dataType);

      if (dataType & kMemoryTypeString) {
        return size / sizeOfOneChar;
      }
            
      if (dataType & kMemoryTypeFloat) {
        return size / sizeOfOneFloat;
      }
            
      if (dataType & kMemoryTypeDouble) {
        return size / sizeOfOneDouble;
      }
      return 0;
    }

    /**
     * @brief Returns a doible* read from shared memory
     * Caller has the obligation to call delete [] on the returning float*.
     *
     * @return float*
     */
    // TODO: might wanna use templated functions here like: <T> readNumericArray()
    inline double* readDoubleArray() {
        void *memory = _memory.data();
        std::size_t size = readSize(kMemoryTypeDouble);
        double* typedMemory = (double*) memory; 

        // allocating memory on heap (this might leak)
        double *data = new double[size / sizeOfOneDouble]();

        // copy to data buffer
        std::memcpy(data, &typedMemory[flagSize + bufferSizeSize], size);
        
        return data;
    }

    /**
     * @brief Returns a float* read from shared memory
     * Caller has the obligation to call delete [] on the returning float*.
     * 
     * @return float* 
     */
    inline float* readFloatArray() {
        void *memory = _memory.data();
        float *typedMemory = (float *)memory;
        
        std::size_t size = readSize(kMemoryTypeFloat);

        // allocating memory on heap (this might leak)
        float *data = new float[size / sizeOfOneFloat]();

        // copy to data buffer
        std::memcpy(data, &typedMemory[flagSize + bufferSizeSize], size);
        
        return data;
    }

    inline std::string readString() {
        char* memory = (char*) _memory.data();

        std::size_t size = readSize(kMemoryTypeString);

        // create a string that copies the data from memory
        std::string data =
            std::string(&memory[flagSize + bufferSizeSize], size);
        
        return data;
    }

private:
    Memory _memory;
};

class SharedMemoryWriteStream {
public:

    SharedMemoryWriteStream(const std::string name, const std::size_t bufferSize, const bool isPersistent): 
        _memory(name, bufferSize, isPersistent) {

        if (_memory.create() != kOK) {
            throw "Shared memory segment could not be created.";
        }
    }

    inline void close() {
      _memory.close();
    }

    // https://stackoverflow.com/questions/18591924/how-to-use-bitmask
    inline char getWriteFlags(const char type,
                              const char currentFlags) {
        char flags = type;

        if ((currentFlags & (kMemoryChanged)) == kMemoryChanged) {
            // disable flag, leave rest untouched
            flags &= ~kMemoryChanged;
        } else {
            // enable flag, leave rest untouched
            flags ^= kMemoryChanged;
        }
        return flags;
    }

    inline void write(const std::string& string) {
        char* memory = (char*) _memory.data();

        // 1) copy change flag into buffer for change detection
        char flags = getWriteFlags(kMemoryTypeString, ((char*) _memory.data())[0]);
        std::memcpy(&memory[0], &flags, flagSize);

        // 2) copy buffer size into buffer (meta data for deserializing)
        const char *stringData = string.data();
        const std::size_t bufferSize = string.size();

        // write data
        std::memcpy(&memory[flagSize], &bufferSize, bufferSizeSize);

        // 3) copy stringData into memory buffer
        std::memcpy(&memory[flagSize + bufferSizeSize], stringData, bufferSize);
    }

    // TODO: might wanna use template function here for numeric arrays,
    // like void writeNumericArray(<T*> data, std::size_t length)
    inline void write(float* data, std::size_t length) {
        float* memory = (float*) _memory.data();

        char flags = getWriteFlags(kMemoryTypeFloat, ((char*) _memory.data())[0]);
        std::memcpy(&memory[0], &flags, flagSize);

        // 2) copy buffer size into buffer (meta data for deserializing)
        const std::size_t bufferSize = length * sizeOfOneFloat;
        std::memcpy(&memory[flagSize], &bufferSize, bufferSizeSize);
        
        // 3) copy float* into memory buffer
        std::memcpy(&memory[flagSize + bufferSizeSize], data, bufferSize);
    }

    inline void write(double* data, std::size_t length) {
        double* memory = (double*) _memory.data();

        char flags = getWriteFlags(kMemoryTypeDouble, ((char*) _memory.data())[0]);
        std::memcpy(&memory[0], &flags, flagSize);

        // 2) copy buffer size into buffer (meta data for deserializing)
        const std::size_t bufferSize = length * sizeOfOneDouble;

        std::memcpy(&memory[flagSize], &bufferSize, bufferSizeSize);
        
        // 3) copy double* into memory buffer
        std::memcpy(&memory[flagSize + bufferSizeSize], data, bufferSize);
    }

    inline void destroy() {
        _memory.destroy();
    }

private:
    Memory _memory;
};


}; // namespace lsm

#endif // INCLUDE_LIBSHAREDMEMORY_HPP_