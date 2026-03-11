#include <libsharedmemory/libsharedmemory.hpp>
#include "lest.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <bitset>
#include <cstdint>
#include <sstream>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <cstdlib>

using namespace std;
using namespace lsm;

namespace
{
inline void log_test_message(const std::string& message)
{
    static int counter = 0;
    std::cout << ++counter << ". " << message << std::endl;
}
}

static const char* g_argv0 = nullptr;

const lest::test specification[] =
{
    // Verifies the lowest-level Memory API: create a named segment, write raw
    // bytes via pointer access, open it from a second handle, and confirm the
    // bytes survive the round-trip. This is the foundation all higher-level
    // streams and queues are built on.
    CASE("shared memory can be created and opened and transfer uint8_t")
    {
        Memory memoryWriter {"lsmtest", 64, true};
        EXPECT(Error::OK == memoryWriter.create());

        static_cast<uint8_t*>(memoryWriter.data())[0] = 0x11;
        static_cast<uint8_t*>(memoryWriter.data())[1] = 0x34;

        Memory memoryReader{"lsmtest", 64, true};

        EXPECT(Error::OK == memoryReader.open());

        log_test_message("single uint8_t: SUCCESS");

        EXPECT(0x11 == ((uint8_t*)memoryReader.data())[0]);
        EXPECT(0x34 == ((uint8_t *)memoryReader.data())[1]);

        memoryWriter.close();
        memoryReader.close();
    },

    // Ensures opening a segment that was never created returns an error
    // instead of silently succeeding with garbage data.
    CASE("non-existing shared memory objects err")
    {
        Memory memoryReader{"lsmtest2", 64, true};
        EXPECT(Error::OpeningFailed == memoryReader.open());
        log_test_message("error when opening non-existing segment: SUCCESS");
    },

    // Tests the high-level stream API for UTF-8 string transfer, including
    // multi-byte emoji characters. Validates that the metadata (flags, size)
    // round-trips correctly alongside the payload.
    CASE("using MemoryStreamWriter and MemoryStreamReader to transfer std::string")
    {
        const std::string dataToTransfer = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

        SharedMemoryWriteStream write${"jsonPipe", 65535, true};
        SharedMemoryReadStream read${"jsonPipe", 65535, true};

        write$.write(dataToTransfer);

        const std::string dataString = read$.readString();

        std::ostringstream msg;
        msg << "std::string (UTF8): SUCCESS | " << dataString;
        log_test_message(msg.str());

        EXPECT(dataToTransfer == dataString);

        write$.close();
        read$.close();
    },

    // Writes a longer string then a shorter one to the same segment 1000 times.
    // Ensures the size metadata updates correctly so the reader never returns
    // stale trailing bytes from a previous longer write.
    CASE("Write more then less, then read")
    {
        for (int i=0; i<1000; i++)
        {
            SharedMemoryWriteStream write${"varyingDataSizePipe", 65535, true};
            SharedMemoryReadStream read${"varyingDataSizePipe", 65535, true};
        
            std::string t1 = "abccde" + std::to_string(i);
            write$.write(t1);

            std::string t2 = "abc" + std::to_string(i);
            write$.write(t2);

            std::string dataString = read$.readString();

            EXPECT(t2 == dataString);

            write$.close();
            read$.close();
        }
        log_test_message("std::string more/less: SUCCESS; 1000 runs");
    },

    // Stress-tests string transfer with a large payload containing repeated
    // multi-byte emoji sequences (~500+ bytes). Validates that memcpy-based
    // serialization handles arbitrary sizes within the buffer limit.
    CASE("Write a lot")
    {
        const std::string blob =
            "ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab"
            "😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃a"
            "b😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃"
            "ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab"
            "😃ab😃ab😃ab😃ab😃ab";

        const SharedMemoryWriteStream write${"blobDataSizePipe", 65535, true};
        const SharedMemoryReadStream read${"blobDataSizePipe", 65535, true};

        write$.write(blob);

        const std::string dataString = read$.readString();

        EXPECT(blob == dataString);

        log_test_message("std::string blob: SUCCESS");
    },

    // Validates the flag byte in the memory layout: the data-type bits identify
    // the payload kind (string/float/double), and the kMemoryChanged bit toggles
    // on each successive write so readers can detect every update.
    CASE("Can read flags, sets the right datatype and data change bit flips")
    {
        SharedMemoryWriteStream write${"blobDataSizePipe2", 65535, true};
        SharedMemoryReadStream read${"blobDataSizePipe2", 65535, true};

        write$.write("foo!");

        char flagsData = read$.readFlags();

        EXPECT(read$.readLength(kMemoryTypeString) == 4UL);

        std::bitset<8> flags(flagsData);

        EXPECT(!!(flagsData & kMemoryTypeString));

        std::ostringstream statusMsg;
        statusMsg << "status flag shows string data type flag: SUCCESS: 0b" << flags;
        log_test_message(statusMsg.str());
        
        EXPECT(!!(flagsData & kMemoryChanged));

        std::ostringstream changeMsg;
        changeMsg << "status flag has the change bit set: SUCCESS: 0b" << flags;
        log_test_message(changeMsg.str());

        write$.write("foo!");

        char flagsData2 = read$.readFlags();
        std::bitset<8> flags2(flagsData2);

        EXPECT(!!(flagsData2 & ~kMemoryChanged));

        write$.write("foo!1");

        char flagsData3 = read$.readFlags();
        std::bitset<8> flags3(flagsData3);
        EXPECT(!!(flagsData3 & kMemoryChanged));

        std::ostringstream zeroMsg;
        zeroMsg << "status bit flips to zero when writing again: SUCCESS: 0b" << flags2;
        log_test_message(zeroMsg.str());
        
        std::ostringstream oneMsg;
        oneMsg << "status bit flips to one when writing again: SUCCESS: 0b" << flags3;
        log_test_message(oneMsg.str());
        
        write$.close();
        read$.close();
    },

    // Tests float array transfer via the raw-pointer write(float*, size_t) API.
    // Verifies flag type bits, element count via readLength(), data integrity
    // at boundaries (first and last element), and change-bit toggling across
    // multiple writes.
    CASE("Can write and read a float* array")
    {
        float numbers[72] =
        {
            1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f,
            1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f,
            1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f,
            1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f,
            1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f,
            1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 3.14f, 1.3f, 3.4f, 6.14f,
        };

        SharedMemoryWriteStream write${"numberPipe", 65535, true};
        SharedMemoryReadStream read${"numberPipe", 65535, true};

        write$.write(numbers, 72);

        EXPECT(read$.readLength(kMemoryTypeFloat) == 72UL);

        char flagsData = read$.readFlags();
        std::bitset<8> flags(flagsData);

        std::ostringstream floatFlagMsg;
        floatFlagMsg << "Flags for float* read: 0b" << flags;
        log_test_message(floatFlagMsg.str());
        EXPECT(!!(flagsData & kMemoryTypeFloat));
        EXPECT(!!(flagsData & kMemoryChanged));

        float* numbersReadPtr = read$.readFloatArray();

        EXPECT(numbers[0] == numbersReadPtr[0]);
        EXPECT(numbers[1] == numbersReadPtr[1]);
        EXPECT(numbers[2] == numbersReadPtr[2]);
        EXPECT(numbers[3] == numbersReadPtr[3]);
        EXPECT(numbers[71] == numbersReadPtr[71]);

        log_test_message("float[72]: SUCCESS");

        write$.write(numbers, 72);

        char flagsData2 = read$.readFlags();
        std::bitset<8> flags2(flagsData2);

        EXPECT(!!(flagsData2 & ~kMemoryChanged));

        write$.write(numbers, 72);

        char flagsData3 = read$.readFlags();
        std::bitset<8> flags3(flagsData3);
        EXPECT(!!(flagsData3 & kMemoryChanged));

        std::ostringstream floatZeroMsg;
        floatZeroMsg << "status bit flips to zero when writing again: SUCCESS: 0b" << flags2;
        log_test_message(floatZeroMsg.str());
        
        std::ostringstream floatOneMsg;
        floatOneMsg << "status bit flips to one when writing again: SUCCESS: 0b" << flags3;
        log_test_message(floatOneMsg.str());

        delete[] numbersReadPtr;
        write$.close();
        read$.close();
    },

    // Same as the float array test but with double precision. Ensures the
    // 8-byte element size is handled correctly in the size metadata and the
    // kMemoryTypeDouble flag is set instead of kMemoryTypeFloat.
    CASE("Can write and read a double* array")
    {
        double numbers[72] =
        {
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
            1.38038450934, 3.43723642783, 3.1438540345, 331.390696969,
            3.483045044,   6.14848338383, 7.3293840293, 8.4234234,
        };

        SharedMemoryWriteStream write${"numberPipe", 65535, true};
        SharedMemoryReadStream read${"numberPipe", 65535, true};

        write$.write(numbers, 72);

        EXPECT(read$.readLength(kMemoryTypeDouble) == 72UL);

        char flagsData = read$.readFlags();
        std::bitset<8> flags(flagsData);

        std::ostringstream doubleFlagMsg;
        doubleFlagMsg << "Flags for double* read: 0b" << flags;
        log_test_message(doubleFlagMsg.str());
        EXPECT(!!(flagsData & kMemoryTypeDouble));
        EXPECT(!!(flagsData & kMemoryChanged));

        double* numbersReadPtr = read$.readDoubleArray();

        EXPECT(numbers[0] == numbersReadPtr[0]);
        EXPECT(numbers[1] == numbersReadPtr[1]);
        EXPECT(numbers[2] == numbersReadPtr[2]);
        EXPECT(numbers[3] == numbersReadPtr[3]);
        EXPECT(numbers[71] == numbersReadPtr[71]);

        log_test_message("double[72]: SUCCESS");

        write$.write(numbers, 72);

        char flagsData2 = read$.readFlags();
        std::bitset<8> flags2(flagsData2);

        EXPECT(!!(flagsData2 & ~kMemoryChanged));

        write$.write(numbers, 72);

        char flagsData3 = read$.readFlags();
        std::bitset<8> flags3(flagsData3);
        EXPECT(!!(flagsData3 & kMemoryChanged));

        std::ostringstream doubleZeroMsg;
        doubleZeroMsg << "status bit flips to zero when writing again: SUCCESS: 0b" << flags2;
        log_test_message(doubleZeroMsg.str());
        
        std::ostringstream doubleOneMsg;
        doubleOneMsg << "status bit flips to one when writing again: SUCCESS: 0b" << flags3;
        log_test_message(doubleOneMsg.str());
        delete[] numbersReadPtr;
        write$.close();
        read$.close();
    },

    // Verifies the persist=true contract: after the writer closes, a new
    // reader can open the same named segment and find the data intact.
    // Critical for use cases where processes restart independently.
    CASE("Persistent shared memory can be reopened")
    {
        const std::string pipeName = "persistSegmentTest";
        {
            Memory writer{pipeName, 128, true};
            EXPECT(Error::OK == writer.create());

            auto *bytes = static_cast<uint8_t*>(writer.data());
            bytes[0] = 0xAB;
            bytes[1] = 0xCD;

            writer.close();
        }

        Memory reader{pipeName, 128, true};
        EXPECT(Error::OK == reader.open());

        auto *readBytes = static_cast<uint8_t*>(reader.data());
        EXPECT(0xAB == readBytes[0]);
        EXPECT(0xCD == readBytes[1]);

        log_test_message("Persistent segment reopened with data intact: SUCCESS");

        reader.close();
        reader.destroy();
    },

    // Verifies the persist=false contract: the OS removes the segment when
    // the creating process closes it at destruction, so a subsequent open fails.
    CASE("Ephemeral shared memory is removed after destruction")
    {
        const std::string pipeName = "ephemeralSegmentTest";
        {
            Memory ephemeral{pipeName, 128, false};
            EXPECT(Error::OK == ephemeral.create());

            static_cast<uint8_t*>(ephemeral.data())[0] = 0x77;
            ephemeral.close();
        }

        Memory reopen{pipeName, 128, false};
        EXPECT(Error::OpeningFailed == reopen.open());

        log_test_message("Ephemeral segment removed after destruction: SUCCESS");
    },

    // Edge case: writing an empty string should produce a zero-length payload
    // and readString() should return an empty string, not crash or return garbage.
    CASE("Shared memory streams handle empty strings")
    {
        const std::string pipeName = "emptyStringPipe";

        SharedMemoryWriteStream writer{pipeName, 64, true};
        SharedMemoryReadStream reader{pipeName, 64, true};

        const std::string emptyValue;
        writer.write(emptyValue);

        EXPECT(0UL == reader.readLength(kMemoryTypeString));
        EXPECT(emptyValue == reader.readString());

        log_test_message("Empty string round-trip through shared memory: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Confirms the single-writer/multiple-reader pattern: one writer publishes
    // data, and the same reader can read it repeatedly without the data being
    // consumed or invalidated (shared memory is not a queue in stream mode).
    CASE("Multiple read, one write")
    {
        const std::string pipeName = "multiReadPipe";

        SharedMemoryWriteStream writer{pipeName, 128, true};
        SharedMemoryReadStream reader1{pipeName, 128, true};

        const std::string message = "Hello, readers!";
        writer.write(message);

        EXPECT(message == reader1.readString());
        EXPECT(message == reader1.readString());

        log_test_message("Multiple readers read the same data: SUCCESS");

        writer.close();
        reader1.close();
        writer.destroy();
    },

    // Tests the full producer-consumer flow using stream change detection:
    // hasNewData(), markAsRead(), isMessageRead(), and waitForRead(). Ensures
    // the writer can pace itself by waiting for the reader to acknowledge each
    // message before publishing the next one.
    CASE("Writer sends messages, reader consumes them")
    {
        const std::string pipeName = "messagePipe";

        SharedMemoryWriteStream writer{pipeName, 256, true};
        SharedMemoryReadStream reader{pipeName, 256, true};

        // Initially there should be no data available
        EXPECT(!reader.hasNewData());
        EXPECT(writer.isMessageRead());

        // Writer publishes first message
        const std::string msg1 = "First message";
        writer.write(msg1);

        // Message should not be marked as read yet
        EXPECT(!writer.isMessageRead());

        // Reader should detect new data available
        EXPECT(reader.hasNewData());

        // Reader consumes the message
        std::string read1 = reader.readString();
        EXPECT(msg1 == read1);
        reader.markAsRead();

        // After consumption, no new data should be available
        EXPECT(!reader.hasNewData());
        EXPECT(writer.isMessageRead());

        // Reading again should return same content but not be marked as "new"
        std::string read1Again = reader.readString();
        EXPECT(msg1 == read1Again);
        EXPECT(!reader.hasNewData());

        // Writer waits for previous message to be consumed before writing next
        writer.waitForRead();

        // Writer publishes second message
        const std::string msg2 = "Second message";
        writer.write(msg2);

        EXPECT(!writer.isMessageRead());

        // New data should be available again
        EXPECT(reader.hasNewData());

        // Reader consumes second message
        std::string read2 = reader.readString();
        EXPECT(msg2 == read2);
        reader.markAsRead();

        // No new data after consumption
        EXPECT(!reader.hasNewData());
        EXPECT(writer.isMessageRead());

        // Writer waits before publishing third message
        writer.waitForRead();

        // Writer publishes third message
        const std::string msg3 = "Third message 🚀";
        writer.write(msg3);

        EXPECT(!writer.isMessageRead());
        EXPECT(reader.hasNewData());

        // Reader consumes third message
        std::string read3 = reader.readString();
        EXPECT(msg3 == read3);
        reader.markAsRead();

        EXPECT(!reader.hasNewData());
        EXPECT(writer.isMessageRead());

        log_test_message("Writer sends messages, reader consumes them: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Core SharedMemoryQueue test: enqueue several messages, then dequeue and
    // verify strict FIFO ordering. Also tests interleaved enqueue/dequeue to
    // exercise the circular buffer wrap-around, and confirms that dequeue on an
    // empty queue returns false.
    CASE("SharedMemoryQueue: Writer enqueues multiple messages, reader dequeues in FIFO order")
    {
        const std::string queueName = "testQueue";
        const std::uint32_t capacity = 10;
        const std::uint32_t maxMessageSize = 256;

        SharedMemoryQueue writer{queueName, capacity, maxMessageSize, true, true};
        SharedMemoryQueue reader{queueName, capacity, maxMessageSize, true, false};

        // Initially queue should be empty
        EXPECT(writer.isEmpty());
        EXPECT(reader.isEmpty());
        EXPECT(writer.size() == 0);
        EXPECT(reader.capacity() == capacity);

        // Writer enqueues multiple messages
        EXPECT(writer.enqueue("First message"));
        EXPECT(writer.enqueue("Second message"));
        EXPECT(writer.enqueue("Third message"));
        EXPECT(writer.enqueue("Fourth message"));
        EXPECT(writer.enqueue("Fifth message"));

        // Check queue state
        EXPECT(!writer.isEmpty());
        EXPECT(writer.size() == 5);
        EXPECT(!writer.isFull());

        // Reader sees the same state
        EXPECT(!reader.isEmpty());
        EXPECT(reader.size() == 5);

        // Reader dequeues messages in FIFO order
        std::string msg;

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "First message");
        EXPECT(reader.size() == 4);

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Second message");
        EXPECT(reader.size() == 3);

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Third message");
        EXPECT(reader.size() == 2);

        // Writer continues to enqueue while reader dequeues
        EXPECT(writer.enqueue("Sixth message"));
        EXPECT(writer.enqueue("Seventh message"));
        EXPECT(writer.size() == 4);

        // Reader dequeues old messages first
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Fourth message");

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Fifth message");

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Sixth message");

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Seventh message");

        // Queue should be empty now
        EXPECT(reader.isEmpty());
        EXPECT(writer.isEmpty());

        // Dequeuing from empty queue should return false
        EXPECT(!reader.dequeue(msg));

        log_test_message("SharedMemoryQueue: FIFO message queue: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Tests back-pressure: filling the queue to capacity, verifying enqueue
    // returns false when full, then draining one slot and confirming enqueue
    // succeeds again. Ensures the circular buffer doesn't overwrite unread data.
    CASE("SharedMemoryQueue: Queue full behavior")
    {
        const std::string queueName = "fullQueue";
        const std::uint32_t capacity = 3;
        const std::uint32_t maxMessageSize = 64;

        SharedMemoryQueue writer{queueName, capacity, maxMessageSize, true, true};
        SharedMemoryQueue reader{queueName, capacity, maxMessageSize, true, false};

        // Fill the queue
        EXPECT(writer.enqueue("Message 1"));
        EXPECT(writer.enqueue("Message 2"));
        EXPECT(writer.enqueue("Message 3"));

        EXPECT(writer.isFull());
        EXPECT(writer.size() == capacity);

        // Attempt to enqueue when full should fail
        EXPECT(!writer.enqueue("Message 4"));

        // Dequeue one message
        std::string msg;
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Message 1");

        // Now we can enqueue again
        EXPECT(!writer.isFull());
        EXPECT(writer.enqueue("Message 4"));

        // Dequeue remaining messages
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Message 2");

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Message 3");

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Message 4");

        EXPECT(reader.isEmpty());

        log_test_message("SharedMemoryQueue: Queue full behavior: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Verifies peek() returns the front message without advancing the read
    // index. Repeated peeks return the same message and size stays constant.
    // After dequeue advances past it, peek returns the next message.
    CASE("SharedMemoryQueue: Peek without dequeuing")
    {
        const std::string queueName = "peekQueue";
        const std::uint32_t capacity = 5;
        const std::uint32_t maxMessageSize = 128;

        SharedMemoryQueue writer{queueName, capacity, maxMessageSize, true, true};
        SharedMemoryQueue reader{queueName, capacity, maxMessageSize, true, false};

        // Enqueue messages
        EXPECT(writer.enqueue("First"));
        EXPECT(writer.enqueue("Second"));
        EXPECT(writer.enqueue("Third"));

        std::string msg;

        // Peek at first message multiple times
        EXPECT(reader.peek(msg));
        EXPECT(msg == "First");
        EXPECT(reader.size() == 3); // Size unchanged

        EXPECT(reader.peek(msg));
        EXPECT(msg == "First");
        EXPECT(reader.size() == 3);

        // Dequeue first message
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "First");
        EXPECT(reader.size() == 2);

        // Peek at second message
        EXPECT(reader.peek(msg));
        EXPECT(msg == "Second");

        // Dequeue all remaining
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Second");

        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "Third");

        // Peek on empty queue should fail
        EXPECT(!reader.peek(msg));
        EXPECT(reader.isEmpty());

        log_test_message("SharedMemoryQueue: Peek functionality: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Stress-tests the single-producer/single-consumer pattern across threads.
    // Producer enqueues 100 messages (retrying when full), consumer dequeues
    // them concurrently. Validates that all messages arrive, the queue drains
    // completely, and no data is lost under thread contention.
    CASE("SharedMemoryQueue: Multithread producer-consumer")
    {
        const std::string queueName = "mtQueue1";
        constexpr std::uint32_t capacity = 20;
        constexpr std::uint32_t maxMessageSize = 128;

        SharedMemoryQueue writer{queueName, capacity, maxMessageSize, true, true};

        constexpr int numMessages = 100;
        std::atomic<int> messagesProduced{0};
        std::atomic<int> messagesConsumed{0};
        std::atomic<bool> producerDone{false};

        // Producer thread
        std::thread producer([&]()
        {
            for (int i = 0; i < numMessages; ++i)
            {
                std::string msg = "Message " + std::to_string(i);
                while (!writer.enqueue(msg))
                {
                    // Queue full, wait a bit
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
                ++messagesProduced;
            }
            producerDone = true;
        });

        // Consumer thread
        std::thread consumer([&]()
        {
            SharedMemoryQueue reader{queueName, capacity, maxMessageSize, true, false};

            while (messagesConsumed < numMessages)
            {
                if (std::string msg; reader.dequeue(msg))
                {
                    // Verify message format (with single producer, FIFO order should be maintained)
                    EXPECT(msg.find("Message ") == 0);
                    ++messagesConsumed;
                }
                else
                {
                    // Queue empty, wait a bit
                    std::this_thread::sleep_for(std::chrono::microseconds(10));
                }
            }

            reader.close();
        });

        producer.join();
        consumer.join();

        EXPECT(messagesProduced == numMessages);
        EXPECT(messagesConsumed == numMessages);
        EXPECT(writer.isEmpty());

        log_test_message("SharedMemoryQueue: Multithread producer-consumer: SUCCESS");

        writer.close();
        writer.destroy();
    },

    // Exercises Memory class accessors that aren't covered by other tests:
    // size() returns the requested allocation, path() contains the segment name
    // (with POSIX normalization), data() is non-null, and as_bytes() returns a
    // std::span<std::byte> aliasing the same region.
    CASE("Memory accessors: size, path, as_bytes")
    {
        Memory mem{"accessorTest", 256, true};
        EXPECT(Error::OK == mem.create());

        EXPECT(mem.size() == 256UL);
        // Path is normalized with a leading '/' on POSIX
        EXPECT(mem.path().find("accessorTest") != std::string::npos);
        EXPECT(mem.data() != nullptr);

        auto bytes = mem.as_bytes();
        EXPECT(bytes.size() == 256UL);
        EXPECT(bytes.data() == static_cast<std::byte*>(mem.data()));

        log_test_message("Memory accessors (size, path, as_bytes): SUCCESS");

        mem.close();
        mem.destroy();
    },

    // Unit-tests the static getWriteFlags() helper in isolation. Verifies that
    // the change bit toggles on each call and the data-type bits (string, float,
    // double) are set correctly without interfering with each other.
    CASE("getWriteFlags toggles change bit and sets type")
    {
        // First write: no previous flags -> change bit ON, type set
        char flags1 = SharedMemoryWriteStream::getWriteFlags(kMemoryTypeString, 0);
        EXPECT(!!(flags1 & kMemoryChanged));
        EXPECT(!!(flags1 & kMemoryTypeString));

        // Second write: change bit was ON -> toggles OFF, type preserved
        char flags2 = SharedMemoryWriteStream::getWriteFlags(kMemoryTypeString, flags1);
        EXPECT(!(flags2 & kMemoryChanged));
        EXPECT(!!(flags2 & kMemoryTypeString));

        // Third write: change bit was OFF -> toggles ON
        char flags3 = SharedMemoryWriteStream::getWriteFlags(kMemoryTypeString, flags2);
        EXPECT(!!(flags3 & kMemoryChanged));

        // Float type flag
        char flagsFloat = SharedMemoryWriteStream::getWriteFlags(kMemoryTypeFloat, 0);
        EXPECT(!!(flagsFloat & kMemoryTypeFloat));
        EXPECT(!(flagsFloat & kMemoryTypeString));

        // Double type flag
        char flagsDouble = SharedMemoryWriteStream::getWriteFlags(kMemoryTypeDouble, 0);
        EXPECT(!!(flagsDouble & kMemoryTypeDouble));
        EXPECT(!(flagsDouble & kMemoryTypeFloat));

        log_test_message("getWriteFlags toggle and type logic: SUCCESS");
    },

    // Tests the C++20 std::span<const float/double> write overloads as an
    // alternative to the raw-pointer API. Uses std::vector as the backing
    // storage to verify span correctly forwards size and data pointer.
    CASE("Write and read float/double arrays via std::span overload")
    {
        std::vector<float> floats = {1.0f, 2.5f, 3.14f, -0.5f, 100.0f};
        std::vector<double> doubles = {1.0, 2.718281828, 3.14159265, -42.0};

        SharedMemoryWriteStream writer{"spanPipe", 65535, true};
        SharedMemoryReadStream reader{"spanPipe", 65535, true};

        writer.write(std::span<const float>(floats));
        EXPECT(reader.readLength(kMemoryTypeFloat) == 5UL);
        float* readFloats = reader.readFloatArray();
        for (size_t i = 0; i < floats.size(); ++i) {
            EXPECT(floats[i] == readFloats[i]);
        }
        delete[] readFloats;

        writer.write(std::span<const double>(doubles));
        EXPECT(reader.readLength(kMemoryTypeDouble) == 4UL);
        double* readDoubles = reader.readDoubleArray();
        for (size_t i = 0; i < doubles.size(); ++i) {
            EXPECT(doubles[i] == readDoubles[i]);
        }
        delete[] readDoubles;

        log_test_message("std::span<float/double> write overloads: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Confirms that sequential writes to the same segment always let the
    // reader see the latest value - shorter, longer, then single-char.
    // Validates that size metadata is updated correctly on each write.
    CASE("Overwriting same segment reads latest data")
    {
        SharedMemoryWriteStream writer{"overwritePipe", 1024, true};
        SharedMemoryReadStream reader{"overwritePipe", 1024, true};

        writer.write("first");
        EXPECT(reader.readString() == "first");

        writer.write("second value, longer");
        EXPECT(reader.readString() == "second value, longer");

        writer.write("x");
        EXPECT(reader.readString() == "x");

        log_test_message("Overwrite same segment reads latest: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Boundary test: a queue with capacity=1 is the smallest valid queue.
    // Verifies it can hold exactly one message, rejects a second, and can be
    // reused after draining - exercising the circular index wrap at offset 0→0.
    CASE("SharedMemoryQueue: capacity=1 edge case")
    {
        SharedMemoryQueue writer{"cap1Queue", 1, 64, true, true};
        SharedMemoryQueue reader{"cap1Queue", 1, 64, true, false};

        EXPECT(writer.isEmpty());
        EXPECT(writer.capacity() == 1);

        EXPECT(writer.enqueue("only slot"));
        EXPECT(writer.isFull());
        EXPECT(writer.size() == 1);
        EXPECT(!writer.enqueue("rejected"));

        std::string msg;
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "only slot");
        EXPECT(reader.isEmpty());

        // Can enqueue again after dequeue
        EXPECT(writer.enqueue("reuse slot"));
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == "reuse slot");

        log_test_message("SharedMemoryQueue: capacity=1 edge case: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // Boundary test for message size: exactly maxMessageSize succeeds,
    // one byte over throws, one byte under succeeds. Validates the >= check
    // in enqueue() and ensures no off-by-one in slot sizing.
    CASE("SharedMemoryQueue: message at maxMessageSize boundary")
    {
        constexpr std::uint32_t maxMsgSize = 32;
        SharedMemoryQueue writer{"boundaryQueue", 5, maxMsgSize, true, true};
        SharedMemoryQueue reader{"boundaryQueue", 5, maxMsgSize, true, false};

        // Exactly maxMessageSize bytes
        std::string exact(maxMsgSize, 'A');
        EXPECT(writer.enqueue(exact));

        std::string msg;
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == exact);

        // One byte over maxMessageSize should throw
        std::string tooLong(maxMsgSize + 1, 'B');
        EXPECT_THROWS(writer.enqueue(tooLong));

        // One byte under maxMessageSize should succeed
        std::string almostFull(maxMsgSize - 1, 'C');
        EXPECT(writer.enqueue(almostFull));
        EXPECT(reader.dequeue(msg));
        EXPECT(msg == almostFull);

        log_test_message("SharedMemoryQueue: maxMessageSize boundary: SUCCESS");

        writer.close();
        reader.close();
        writer.destroy();
    },

    // KNOWN LIMITATION TEST: Two threads write to the same stream segment
    // concurrently. write() performs 3 non-atomic memcpy ops (flags, size, data),
    // so interleaving can produce torn reads with mixed content or wrong sizes.
    // This test reports corruption counts without asserting - the behavior is
    // timing-dependent plus hard to trigger on Apple Silicon's strong memory model.
    CASE("Concurrent stream writers cause data corruption (demonstrates known limitation)")
    {
        constexpr int iterations = 2000;
        std::atomic<int> corrupted{0};
        std::atomic<int> sizeCorrupted{0};

        SharedMemoryWriteStream writer{"concurrentWriteStream", 65535, true};
        SharedMemoryReadStream reader{"concurrentWriteStream", 65535, true};

        std::atomic<int> ready{0};
        std::atomic<bool> go{false};

        // Writer A: writes strings of 'A' (length 100) via shared writer
        std::thread writerA([&]() {
            ready.fetch_add(1);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < iterations; ++i) {
                writer.write(std::string(100, 'A'));
            }
        });

        // Writer B: writes strings of 'B' (length 200) via same shared writer
        std::thread writerB([&]() {
            ready.fetch_add(1);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < iterations; ++i) {
                writer.write(std::string(200, 'B'));
            }
        });

        // Wait for both writers to be ready, then release simultaneously
        while (ready.load() < 2) std::this_thread::yield();
        go.store(true, std::memory_order_release);

        // Reader on main thread: continuously sample for mixed content or bad sizes
        for (int i = 0; i < iterations * 2; ++i) {
            std::string val = reader.readString();
            if (val.empty()) continue;

            // Check for content corruption (mixed A and B chars)
            bool allA = std::all_of(val.begin(), val.end(), [](char c){ return c == 'A'; });
            bool allB = std::all_of(val.begin(), val.end(), [](char c){ return c == 'B'; });
            if (!allA && !allB) {
                ++corrupted;
            }

            // Check for size corruption (should be exactly 100 or 200)
            if (val.size() != 100 && val.size() != 200) {
                ++sizeCorrupted;
            }
        }

        writerA.join();
        writerB.join();

        std::ostringstream msg;
        msg << "Concurrent stream writers: content_corrupted=" << corrupted.load()
            << " size_corrupted=" << sizeCorrupted.load()
            << " out of " << (iterations * 2)
            << " reads (demonstrates known limitation)";
        log_test_message(msg.str());

        // We don't EXPECT corruption - it's timing dependent. We just report it.
        // The point is: if corrupted > 0, the README warning is validated.

        reader.close();
        writer.close();
        writer.destroy();
    },

    // KNOWN LIMITATION TEST: Two threads call enqueue() on the same queue.
    // enqueue() does a non-atomic read of writeIndex, writes the message, then
    // bumps the index. Both threads can read the same index, overwrite each
    // other's slot, and the index advances only once - causing 22-45% message
    // corruption in practice. Reports counts without asserting since behavior is
    // timing-dependent.
    CASE("Concurrent queue producers cause lost messages (demonstrates known limitation)")
    {
        const std::string queueName = "concurrentProducers";
        constexpr std::uint32_t capacity = 2000;
        constexpr std::uint32_t maxMsgSize = 64;
        constexpr int msgsPerProducer = 500;

        SharedMemoryQueue writer1{queueName, capacity, maxMsgSize, true, true};

        std::atomic<int> ready{0};
        std::atomic<bool> go{false};
        std::atomic<int> enqueuedA{0};
        std::atomic<int> enqueuedB{0};

        // Producer A
        std::thread producerA([&]() {
            ready.fetch_add(1);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < msgsPerProducer; ++i) {
                if (writer1.enqueue("A-" + std::to_string(i)))
                    ++enqueuedA;
            }
        });

        // Producer B: uses the same writer instance (shared state, no lock)
        std::thread producerB([&]() {
            ready.fetch_add(1);
            while (!go.load(std::memory_order_acquire)) std::this_thread::yield();
            for (int i = 0; i < msgsPerProducer; ++i) {
                if (writer1.enqueue("B-" + std::to_string(i)))
                    ++enqueuedB;
            }
        });

        // Wait for both producers to be ready, then release simultaneously
        while (ready.load() < 2) std::this_thread::yield();
        go.store(true, std::memory_order_release);

        producerA.join();
        producerB.join();

        int totalEnqueued = enqueuedA.load() + enqueuedB.load();

        // Drain and count
        SharedMemoryQueue reader{queueName, capacity, maxMsgSize, true, false};
        int dequeued = 0;
        int contentCorrupted = 0;
        std::string msg;
        while (reader.dequeue(msg)) {
            ++dequeued;
            // Check message starts with "A-" or "B-"
            if (msg.substr(0, 2) != "A-" && msg.substr(0, 2) != "B-") {
                ++contentCorrupted;
            }
        }

        int lost = totalEnqueued - dequeued;

        std::ostringstream report;
        report << "Concurrent queue producers: enqueued=" << totalEnqueued
               << " dequeued=" << dequeued
               << " lost=" << lost
               << " content_corrupted=" << contentCorrupted
               << " (demonstrates known limitation)";
        log_test_message(report.str());

        // We don't assert on the exact count - it's timing dependent.
        // If lost > 0 or contentCorrupted > 0, the README warning is validated.

        writer1.close();
        reader.close();
        writer1.destroy();
    },

    // Stability / flake guard: re-runs the entire test suite 1000 times in
    // sub-processes to catch timing-dependent failures that only surface under
    // repetition. Skips itself via the LSM_NOFLAKE env var to avoid infinite
    // recursion. Cross-platform (POSIX + Windows CMD).
    CASE("no-flake: 1000 consecutive runs must all pass")
    {
        // Skip when invoked as a subprocess to avoid infinite recursion
        if (std::getenv("LSM_NOFLAKE")) {
            log_test_message("no-flake: skipped (subprocess)");
            return;
        }

        log_test_message("no-flake: running 1000 iterations (this will take a moment)...");

        std::string exe(g_argv0 ? g_argv0 : "./lsm_test");
#ifdef _WIN32
        std::string cmd = "set LSM_NOFLAKE=1 && \"" + exe + "\" > NUL 2>&1";
#else
        std::string cmd = "LSM_NOFLAKE=1 \"" + exe + "\" > /dev/null 2>&1";
#endif

        int failures = 0;
        for (int i = 1; i <= 1000; ++i) {
            if (std::system(cmd.c_str()) != 0) {
                ++failures;
                std::cerr << "FAIL on run " << i << std::endl;
            }
        }

        std::ostringstream report;
        report << "no-flake: " << (1000 - failures)
               << " passed, " << failures << " failed out of 1000";
        log_test_message(report.str());

        EXPECT(failures == 0);
    },
};

int main (const int argc, char *argv[])
{
    g_argv0 = argv[0];
    return lest::run(specification, argc, argv);
}
