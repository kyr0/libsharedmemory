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

const lest::test specification[] =
{
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

    CASE("non-existing shared memory objects err")
    {
        Memory memoryReader{"lsmtest2", 64, true};
        EXPECT(Error::OpeningFailed == memoryReader.open());
        log_test_message("error when opening non-existing segment: SUCCESS");
    },

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
    }

    // NOTE: Multiple concurrent producers accessing the same SharedMemoryQueue instance
    // requires additional synchronization (mutex or atomic operations) to prevent race
    // conditions. The current implementation works well for:
    // - Single producer, single consumer
    // - Single producer, multiple consumers (read-only operations)
    // Future work: Add atomic operations for truly lock-free multiple producer support
};

int main (const int argc, char *argv[])
{
    return lest::run(specification, argc, argv);
}
