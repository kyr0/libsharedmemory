#include <libsharedmemory/libsharedmemory.hpp>
#include "lest.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <bitset>
#include <cstdint>
#include <sstream>

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
};

int main (const int argc, char *argv[])
{
    return lest::run(specification, argc, argv);
}
