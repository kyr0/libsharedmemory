#pragma once

#define LIBSHAREDMEMORY_VERSION_MAJOR 1
#define LIBSHAREDMEMORY_VERSION_MINOR 6
#define LIBSHAREDMEMORY_VERSION_PATCH 0

#include <ostream>
#include <cstring>
#include <string>
#include <string_view>
#include <cstddef> // nullptr_t, ptrdiff_t, std::size_t
#include <limits>
#include <span>
#include <thread>
#include <stdexcept>
#include <atomic> // added for atomic queue counters

#if defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION) || defined(__ANDROID__)
#include <fcntl.h>    // O_* constants
#include <sys/mman.h> // mmap, munmap
#include <unistd.h>   // shm functions, close
#endif

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#undef min
#undef max
#undef WIN32_LEAN_AND_MEAN
#include <filesystem>
#include <utility>
#endif

namespace lsm
{

enum class Error
{
  OK = 0,
  CreationFailed = 100,
  MappingFailed = 110,
  OpeningFailed = 120,
};

enum DataType : std::uint8_t
{
  kMemoryChanged = 1,
  kMemoryTypeString = 2,
  kMemoryTypeFloat = 4,
  kMemoryTypeDouble = 8,
};

// byte sizes of memory layout
inline constexpr std::size_t bufferSizeSize = 4; // store buffer length as 32-bit value
inline constexpr std::size_t sizeOfOneFloat = 4; // float takes 4 bytes
inline constexpr std::size_t sizeOfOneChar = 1; // char takes 1 byte
inline constexpr std::size_t sizeOfOneDouble = 8; // double takes 8 bytes
inline constexpr std::size_t flagSize = 1; // char takes 1 byte

class Memory
{
public:
    // path should only contain alpha-numeric characters, and is normalized
    // on linux/macOS.
    explicit Memory(const std::string& path, std::size_t size, bool persist);

    // create a shared memory area and open it for writing
    [[nodiscard]] Error create()
    {
        return createOrOpen(true);
    }

    // open an existing shared memory for reading
    [[nodiscard]] Error open()
    {
        return createOrOpen(false);
    }

    [[nodiscard]] std::size_t size() const noexcept
    {
        return _size;
    }

    [[nodiscard]] const std::string &path() const noexcept
    {
        return _path;
    }

    [[nodiscard]] void *data() const noexcept
    {
        return _data;
    }

    [[nodiscard]] std::span<std::byte> as_bytes() const noexcept
    {
        return {static_cast<std::byte*>(_data), _size};
    }

    void destroy() const;

    void close();

    ~Memory();

private:
    [[nodiscard]] Error createOrOpen(bool create);

    std::string _path;
    void *_data = nullptr;
    std::size_t _size = 0;
    bool _persist = true;
#if defined(_WIN32)
    HANDLE _handle = nullptr;
    HANDLE _fileHandle = INVALID_HANDLE_VALUE;
    std::string _persistFilePath;
#else
    int _fd = -1;
#endif
};

// Windows shared memory implementation
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

#include <io.h>  // CreateFileMappingA, OpenFileMappingA, etc.

namespace lsm_windows_detail
{
    inline std::string GetSystemStorageDirectory()
    {
        char* programData = nullptr;
        size_t len = 0;
        if (_dupenv_s(&programData, &len, "PROGRAMDATA") == 0 && programData != nullptr)
        {
            std::filesystem::path storagePath = std::filesystem::path(programData) / "shared_memory";
            free(programData);

            // Create directory if it doesn't exist
            std::error_code ec;
            std::filesystem::create_directories(storagePath, ec);
            if (ec)
            {
                // Directory creation failed
                return {};
            }

            return storagePath.string();
        }
        // If PROGRAMDATA is not set, return empty string
        return {};
    }

    inline std::string sanitize_name(const std::string& name)
    {
        std::string sanitized = name;
        const std::string invalid = "\\/:*?\"<>|";
        for (size_t idx = 0; idx < sanitized.size(); ++idx)
        {
            const char ch = sanitized[idx];
            if (ch < 32 || invalid.find(ch) != std::string::npos)
            {
                sanitized[idx] = '_';
            }
        }
        return sanitized;
    }

    inline std::string persistence_file_path(const std::string& name)
    {
        std::string basePath = GetSystemStorageDirectory();
        if (!basePath.empty())
        {
            if (const char last = basePath[basePath.size() - 1];
                last != '\\' && last != '/')
            {
                basePath.push_back('\\');
            }
        }
        basePath += "lsm_";
        basePath += sanitize_name(name);
        basePath += ".shm";
        return basePath;
    }
}

Memory::Memory(const std::string& path, std::size_t size, bool persist) : _path(path), _size(size), _persist(persist)
{
    if (_persist)
    {
        _persistFilePath = lsm_windows_detail::persistence_file_path(_path);
    }
}

Error Memory::createOrOpen(const bool create)
{
    const DWORD size_high_order = static_cast<DWORD>((static_cast<unsigned long long>(_size) >> 32) & 0xFFFFFFFFull);
    const DWORD size_low_order = static_cast<DWORD>(static_cast<unsigned long long>(_size) & 0xFFFFFFFFull);

    if (_persist)
    {
        if (_persistFilePath.empty())
        {
            _persistFilePath = lsm_windows_detail::persistence_file_path(_path);
        }

        // OPEN_ALWAYS: Opens if exists, creates if not - allows for persistent reuse
        // CREATE_ALWAYS: Always creates new, truncating existing - forces fresh start
        const DWORD disposition = create ? CREATE_ALWAYS : OPEN_EXISTING;
        HANDLE fileHandle = CreateFileA(_persistFilePath.c_str(),
                                        GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL,
                                        disposition,
                                        FILE_ATTRIBUTE_NORMAL,
                                        NULL);

        if (fileHandle == INVALID_HANDLE_VALUE)
        {
            return create ? Error::CreationFailed : Error::OpeningFailed;
        }

        LARGE_INTEGER requiredSize;
        requiredSize.QuadPart = static_cast<LONGLONG>(_size);

        // Always resize to required size in create mode to ensure proper initialization
        bool resizeFile = create;
        if (!create)
        {
            LARGE_INTEGER currentSize;
            if (GetFileSizeEx(fileHandle, &currentSize))
            {
                resizeFile = currentSize.QuadPart < requiredSize.QuadPart;
            }
        }

        if (resizeFile)
        {
            if (!SetFilePointerEx(fileHandle, requiredSize, NULL, FILE_BEGIN) || !SetEndOfFile(fileHandle))
            {
                CloseHandle(fileHandle);
                return Error::CreationFailed;
            }
        }

        _fileHandle = fileHandle;

        _handle = CreateFileMappingA(_fileHandle,
                                     NULL,
                                     PAGE_READWRITE,
                                     size_high_order,
                                     size_low_order,
                                     NULL);

        if (!_handle)
        {
            CloseHandle(_fileHandle);
            _fileHandle = INVALID_HANDLE_VALUE;
            return Error::MappingFailed;
        }
    }
    else
    {
        if (create)
        {
            // For ephemeral memory, try to create with the name
            // If it already exists, we'll get a handle to the existing one
            _handle = CreateFileMappingA(INVALID_HANDLE_VALUE,  // use paging file
                                         NULL,                  // default security
                                         PAGE_READWRITE,        // read/write access
                                         size_high_order, size_low_order,
                                         _path.c_str());        // name of mapping object

            if (!_handle)
            {
                return Error::CreationFailed;
            }

            // Check if we opened an existing mapping (can happen in multi-process scenarios)
            // If GetLastError() returns ERROR_ALREADY_EXISTS, the mapping already existed
            // For ephemeral memory in create mode, this is typically fine since the
            // destructor will clean it up when all processes are done
        }
        else
        {
            _handle = OpenFileMappingA(FILE_MAP_ALL_ACCESS, // read/write access
                                       FALSE,               // do not inherit the name
                                       _path.c_str());      // name of mapping object

            if (!_handle)
            {
                return Error::OpeningFailed;
            }
        }
    }

    // Change detection relies on explicit flags to keep the implementation lightweight

    const DWORD access = FILE_MAP_ALL_ACCESS; // always request read/write view
    _data = MapViewOfFile(_handle, access, 0, 0, _size);

    if (!_data)
    {
        close();
        return Error::MappingFailed;
    }
    return Error::OK;
}

void Memory::destroy() const
{
    if (_persistFilePath.empty())
    {
        return;
    }

    if (!DeleteFileA(_persistFilePath.c_str()))
    {
        MoveFileExA(_persistFilePath.c_str(), NULL, MOVEFILE_DELAY_UNTIL_REBOOT);
    }
}

void Memory::close()
{
    if (_data)
    {
        UnmapViewOfFile(_data);
        _data = nullptr;
    }
    if (_handle)
    {
        CloseHandle(_handle);
        _handle = nullptr;
    }
    if (_fileHandle != INVALID_HANDLE_VALUE)
    {
        CloseHandle(_fileHandle);
        _fileHandle = INVALID_HANDLE_VALUE;
    }
}

Memory::~Memory()
{
    close();
    if (!_persist)
    {
        destroy();
    }
}
#endif // defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

// POSIX shared memory implementation
#if defined(__APPLE__) || defined(__linux__) || defined(__unix__) || defined(_POSIX_VERSION) || defined(__ANDROID__)

inline Memory::Memory(const std::string& path, const std::size_t size, const bool persist) : _size(size), _persist(persist)
{
    _path = "/" + path;
}

inline Error Memory::createOrOpen(const bool create)
{
    if (create)
    {
        // shm segments persist across runs, and macOS will refuse
        // to ftruncate an existing shm segment, so to be on the safe
        // side, we unlink it beforehand.
        const int ret = shm_unlink(_path.c_str());
        if (ret < 0)
        {
            if (errno != ENOENT)
            {
                return Error::CreationFailed;
            }
        }
    }

    const int flags = create ? (O_CREAT | O_RDWR) : O_RDWR;

    _fd = shm_open(_path.c_str(), flags, 0755);
    if (_fd < 0)
    {
        if (create)
        {
            return Error::CreationFailed;
        }
        else
        {
            return Error::OpeningFailed;
        }
    }

    if (create)
    {
        // this is the only way to specify the size of a
        // newly-created POSIX shared memory object
        const int ret = ftruncate(_fd, static_cast<off_t>(_size));
        if (ret != 0)
        {
            return Error::CreationFailed;
        }
    }

    constexpr int prot = PROT_READ | PROT_WRITE;

    _data = mmap(nullptr,    // addr
                 _size,      // length
                 prot,       // prot
                 MAP_SHARED, // flags
                 _fd,        // fd
                 0           // offset
    );

    if (_data == MAP_FAILED)
    {
        return Error::MappingFailed;
    }

    if (!_data)
    {
        return Error::MappingFailed;
    }
    return Error::OK;
}

inline void Memory::destroy() const
{
    shm_unlink(_path.c_str());
}

inline void Memory::close()
{
    munmap(_data, _size);
    if (_fd >= 0)
    {
        const int fd_to_close = _fd;
        _fd = -1;
        // call POSIX close directly to avoid name collision with method close()
        ::close(fd_to_close);
    }
}

inline Memory::~Memory()
{
    close();
    if (!_persist)
    {
        destroy();
    }
}

#endif // POSIX implementation

class SharedMemoryReadStream
{
public:
    SharedMemoryReadStream(const std::string& name, const std::size_t bufferSize, const bool isPersistent):
        _memory(name, bufferSize, isPersistent)
    {
        if (_memory.open() != Error::OK)
        {
            throw std::runtime_error("Shared memory segment could not be opened.");
        }
    }

    [[nodiscard]] char readFlags() const noexcept
    {
        const auto memory = static_cast<const char*>(_memory.data());
        return memory[0];
    }

    [[nodiscard]] bool hasNewData() const noexcept
    {
        const char flags = readFlags();
        return !!(flags & kMemoryChanged);
    }

    void markAsRead() const noexcept
    {
        auto memory = static_cast<char*>(_memory.data());
        memory[0] &= ~kMemoryChanged;
    }

    void close()
    {
        _memory.close();
    }

    [[nodiscard]] size_t readSize(const char /*dataType*/) const noexcept
    {
        const auto memory = static_cast<const char*>(_memory.data());
        std::uint32_t storedSize = 0;
        std::memcpy(&storedSize, &memory[flagSize], bufferSizeSize);
        return static_cast<std::size_t>(storedSize);
    }

    [[nodiscard]] size_t readLength(const char dataType) const noexcept
    {
        const size_t size = readSize(dataType);

        if (dataType & kMemoryTypeString)
        {
            return size / sizeOfOneChar;
        }

        if (dataType & kMemoryTypeFloat)
        {
            return size / sizeOfOneFloat;
        }

        if (dataType & kMemoryTypeDouble)
        {
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
    [[nodiscard]] double* readDoubleArray() const
    {
        return readNumericArray<double>(kMemoryTypeDouble, sizeOfOneDouble);
    }

    /**
     * @brief Returns a float* read from shared memory
     * Caller has the obligation to call delete [] on the returning float*.
     *
     * @return float*
     */
    [[nodiscard]] float* readFloatArray() const
    {
        return readNumericArray<float>(kMemoryTypeFloat, sizeOfOneFloat);
    }

    [[nodiscard]] std::string readString() const
    {
        const auto memory = static_cast<const char*>(_memory.data());

        const std::size_t size = readSize(kMemoryTypeString);

        // create a string that copies the data from memory
        auto data = std::string(&memory[flagSize + bufferSizeSize], size);

        return data;
    }

private:
    template <typename T>
    [[nodiscard]] T* readNumericArray(const char typeFlag, const std::size_t elementSize) const
    {
        const auto memory = static_cast<const char*>(_memory.data());
        const std::size_t byteSize = readSize(typeFlag);
        const std::size_t length = byteSize / elementSize;

    auto data = new T[length]();
        std::memcpy(data, &memory[flagSize + bufferSizeSize], byteSize);

        return data;
    }

    Memory _memory;
};

class SharedMemoryWriteStream
{
public:
    SharedMemoryWriteStream(const std::string& name, const std::size_t bufferSize, const bool isPersistent):
        _memory(name, bufferSize, isPersistent)
    {
        if (_memory.create() != Error::OK)
        {
            throw std::runtime_error("Shared memory segment could not be created.");
        }
    }

    void close()
    {
        _memory.close();
    }

    [[nodiscard]] bool isMessageRead() const noexcept
    {
        const auto memory = static_cast<const char*>(_memory.data());
        const char flags = memory[0];
        return !(flags & kMemoryChanged);
    }

    void waitForRead() const noexcept
    {
        while (!isMessageRead())
        {
            std::this_thread::yield();
        }
    }

    // https://stackoverflow.com/questions/18591924/how-to-use-bitmask
    [[nodiscard]] static constexpr char getWriteFlags(const char type, const char currentFlags) noexcept
    {
        char flags = type;

        if ((currentFlags & (kMemoryChanged)) == kMemoryChanged)
        {
            // disable flag, leave rest untouched
            flags &= ~kMemoryChanged;
        }
        else
        {
            // enable flag, leave rest untouched
            flags ^= kMemoryChanged;
        }
        return flags;
    }

    void write(std::string_view string) const
    {
        const auto memory = static_cast<char*>(_memory.data());

        if (string.size() > std::numeric_limits<std::uint32_t>::max())
        {
            throw std::runtime_error("String payload exceeds maximum shared memory size.");
        }

        // 1) copy change flag into buffer for change detection
        const char flags = getWriteFlags(kMemoryTypeString, memory[0]);
        std::memcpy(&memory[0], &flags, flagSize);

        // 2) copy buffer size into buffer (meta data for deserializing)
        const char *stringData = string.data();
        const auto bufferSize = static_cast<std::uint32_t>(string.size());

        // write data
        std::memcpy(&memory[flagSize], &bufferSize, bufferSizeSize);

        // 3) copy stringData into memory buffer
        std::memcpy(&memory[flagSize + bufferSizeSize], stringData, bufferSize);
    }

    void write(std::span<const float> data) const
    {
        writeNumericArray(data, kMemoryTypeFloat);
    }

    void write(const float* data, const std::size_t length) const
    {
        write(std::span<const float>(data, length));
    }

    void write(std::span<const double> data) const
    {
        writeNumericArray(data, kMemoryTypeDouble);
    }

    void write(const double* data, const std::size_t length) const
    {
        write(std::span<const double>(data, length));
    }

    void destroy() const
    {
        _memory.destroy();
    }

private:
    template <typename T>
    requires std::is_floating_point_v<T>
    void writeNumericArray(std::span<const T> data, const char typeFlag) const
    {
        const std::size_t length = data.size();

        if (length > 0 && length > (std::numeric_limits<std::uint32_t>::max() / sizeof(T)))
        {
            throw std::runtime_error("Numeric payload exceeds maximum shared memory size.");
        }

        const auto memory = static_cast<char*>(_memory.data());

        const char flags = getWriteFlags(typeFlag, memory[0]);
        std::memcpy(&memory[0], &flags, flagSize);

        const auto bufferSize = static_cast<std::uint32_t>(length * sizeof(T));
        std::memcpy(&memory[flagSize], &bufferSize, bufferSizeSize);
        std::memcpy(&memory[flagSize + bufferSizeSize], data.data(), bufferSize);
    }

    Memory _memory;
};

/**
 * @brief Queue structure for shared memory
 * Layout: [writeIndex(4)][readIndex(4)][capacity(4)][count(4)][maxMessageSize(4)][messages...]
 */
class SharedMemoryQueue
{
private:
    static constexpr std::size_t kWriteIndexOffset = 0;
    static constexpr std::size_t kReadIndexOffset = 4;
    static constexpr std::size_t kCapacityOffset = 8;
    static constexpr std::size_t kCountOffset = 12;
    static constexpr std::size_t kMaxMessageSizeOffset = 16;
    static constexpr std::size_t kHeaderSize = 20;

    Memory _memory;
    std::uint32_t _capacity;
    std::uint32_t _maxMessageSize;
    bool _isWriter;

    [[nodiscard]] std::uint32_t readUInt32(std::size_t offset) const noexcept
    {
        const auto memory = static_cast<const char*>(_memory.data());
        std::uint32_t value = 0;
        std::memcpy(&value, &memory[offset], sizeof(std::uint32_t));
        return value;
    }

    void writeUInt32(std::size_t offset, std::uint32_t value) const noexcept
    {
        auto memory = static_cast<char*>(_memory.data());
        std::memcpy(&memory[offset], &value, sizeof(std::uint32_t));
    }

    [[nodiscard]] std::size_t getMessageOffset(std::uint32_t index) const noexcept
    {
        // Each slot contains: [length(4)][data(maxMessageSize)]
        return kHeaderSize + index * (_maxMessageSize + sizeof(std::uint32_t));
    }

    // Helper to access atomic count field in shared memory
    [[nodiscard]] std::atomic<std::uint32_t>& atomicCount() const noexcept
    {
        auto memory = static_cast<char*>(_memory.data());
        return *reinterpret_cast<std::atomic<std::uint32_t>*>(&memory[kCountOffset]);
    }

public:
    /**
     * @brief Create or open a shared memory queue
     * @param name Queue name
     * @param capacity Maximum number of messages in queue
     * @param maxMessageSize Maximum size of each message in bytes
     * @param isPersistent Whether the queue persists after process exit
     * @param isWriter True to create/write, false to open/read
     */
    SharedMemoryQueue(const std::string& name, std::uint32_t capacity,
                      std::uint32_t maxMessageSize, bool isPersistent, bool isWriter)
        : _memory(name, kHeaderSize + capacity * (maxMessageSize + sizeof(std::uint32_t)), isPersistent)
        , _capacity(capacity)
        , _maxMessageSize(maxMessageSize)
        , _isWriter(isWriter)
    {
        if (isWriter)
        {
            if (_memory.create() != Error::OK)
            {
                throw std::runtime_error("Shared memory queue could not be created.");
            }

            // Initialize queue metadata
            writeUInt32(kWriteIndexOffset, 0);
            writeUInt32(kReadIndexOffset, 0);
            writeUInt32(kCapacityOffset, capacity);
            // construct atomic count with placement new to ensure proper atomic object initialization
            auto memory = static_cast<char*>(_memory.data());
            new (&memory[kCountOffset]) std::atomic<std::uint32_t>(0);
            writeUInt32(kMaxMessageSizeOffset, maxMessageSize);
        }
        else
        {
            if (_memory.open() != Error::OK)
            {
                throw std::runtime_error("Shared memory queue could not be opened.");
            }

            // Read queue metadata
            _capacity = readUInt32(kCapacityOffset);
            _maxMessageSize = readUInt32(kMaxMessageSizeOffset);
        }
    }

    [[nodiscard]] bool isEmpty() const noexcept
    {
        return atomicCount().load(std::memory_order_acquire) == 0;
    }

    [[nodiscard]] bool isFull() const noexcept
    {
        return atomicCount().load(std::memory_order_acquire) >= _capacity;
    }

    [[nodiscard]] std::uint32_t size() const noexcept
    {
        return atomicCount().load(std::memory_order_acquire);
    }

    [[nodiscard]] std::uint32_t capacity() const noexcept
    {
        return _capacity;
    }

    /**
     * @brief Enqueue a message (writer only)
     * @param message Message to enqueue
     * @return true if message was enqueued, false if queue is full
     */
    bool enqueue(std::string_view message)
    {
        if (!_isWriter)
        {
            throw std::runtime_error("Cannot enqueue from a reader queue instance.");
        }

        if (message.size() > _maxMessageSize)
        {
            throw std::runtime_error("Message exceeds maximum message size.");
        }

        if (isFull())
        {
            return false;
        }

        const std::uint32_t writeIndex = readUInt32(kWriteIndexOffset);
        const std::size_t offset = getMessageOffset(writeIndex);

        auto memory = static_cast<char*>(_memory.data());

        // Write message length
        const auto messageLength = static_cast<std::uint32_t>(message.size());
        std::memcpy(&memory[offset], &messageLength, sizeof(std::uint32_t));

        // Write message data
        std::memcpy(&memory[offset + sizeof(std::uint32_t)], message.data(), messageLength);

        // Update write index (circular)
        const std::uint32_t newWriteIndex = (writeIndex + 1) % _capacity;
        writeUInt32(kWriteIndexOffset, newWriteIndex);

        // atomic increment of count
        atomicCount().fetch_add(1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Dequeue a message (reader only)
     * @param message Output parameter for dequeued message
     * @return true if message was dequeued, false if queue is empty
     */
    bool dequeue(std::string& message)
    {
        if (_isWriter)
        {
            throw std::runtime_error("Cannot dequeue from a writer queue instance.");
        }

        if (isEmpty())
        {
            return false;
        }

        const std::uint32_t readIndex = readUInt32(kReadIndexOffset);
        const std::size_t offset = getMessageOffset(readIndex);

        const auto memory = static_cast<const char*>(_memory.data());

        // Read message length
        std::uint32_t messageLength = 0;
        std::memcpy(&messageLength, &memory[offset], sizeof(std::uint32_t));

        // Read message data
        message.resize(messageLength);
        std::memcpy(&message[0], &memory[offset + sizeof(std::uint32_t)], messageLength);

        // Update read index (circular)
        const std::uint32_t newReadIndex = (readIndex + 1) % _capacity;
        writeUInt32(kReadIndexOffset, newReadIndex);

        // atomic decrement of count
        atomicCount().fetch_sub(1, std::memory_order_release);

        return true;
    }

    /**
     * @brief Peek at the next message without dequeuing (reader only)
     * @param message Output parameter for peeked message
     * @return true if message was peeked, false if queue is empty
     */
    bool peek(std::string& message) const
    {
        if (_isWriter)
        {
            throw std::runtime_error("Cannot peek from a writer queue instance.");
        }

        if (isEmpty())
        {
            return false;
        }

        const std::uint32_t readIndex = readUInt32(kReadIndexOffset);
        const std::size_t offset = getMessageOffset(readIndex);

        const auto memory = static_cast<const char*>(_memory.data());

        // Read message length
        std::uint32_t messageLength = 0;
        std::memcpy(&messageLength, &memory[offset], sizeof(std::uint32_t));

        // Read message data
        message.resize(messageLength);
        std::memcpy(&message[0], &memory[offset + sizeof(std::uint32_t)], messageLength);

        return true;
    }

    void close()
    {
        _memory.close();
    }

    void destroy() const
    {
        _memory.destroy();
    }
};

}; // namespace lsm
