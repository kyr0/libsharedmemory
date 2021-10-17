#include <libsharedmemory/libsharedmemory.hpp>
#include "lest.hpp"
#include <iostream>
#include <ostream>
#include <string>
#include <bitset>

using namespace std;
using namespace lsm;

const lest::test specification[] = {
    CASE("shared memory can be created and opened and transfer uint8_t") {
        Memory memoryWriter {"lsmtest", 64, true};
        EXPECT(kOK == memoryWriter.create());

        ((uint8_t*)memoryWriter.data())[0] = 0x11;
        ((uint8_t*)memoryWriter.data())[1] = 0x34;

        Memory memoryReader{"lsmtest", 64, true};

        EXPECT(kOK == memoryReader.open());

        std::cout << "1. single uint8_t: SUCCESS" << std::endl;

        EXPECT(0x11 == ((uint8_t*)memoryReader.data())[0]);
        EXPECT(0x34 == ((uint8_t *)memoryReader.data())[1]);

        memoryWriter.close();
        memoryReader.close();
    },

    CASE("non-existing shared memory objects err") {
        Memory memoryReader{"lsmtest2", 64, true};
        EXPECT(kErrorOpeningFailed == memoryReader.open());
        std::cout << "2. error when opening non-existing segment: SUCCESS" << std::endl;
    },

    CASE("using MemoryStreamWriter and MemoryStreamReader to transfer std::string") {

        std::string dataToTransfer = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

        SharedMemoryWriteStream write${"jsonPipe", 65535, true};
        SharedMemoryReadStream read${"jsonPipe", 65535, true};

        write$.write(dataToTransfer);

        std::string dataString = read$.readString();

        std::cout << "3. std::string (UTF8): SUCCESS | " << dataString << std::endl;

        EXPECT(dataToTransfer == dataString);

        write$.close();
        read$.close();
    }
    ,

    CASE("Write more then less, then read") {

        for (int i=0; i<1000; i++) {
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
        std::cout << "4. std::string more/less: SUCCESS; 1000 runs"
                  << std::endl;
        
    },

    CASE("Write a lot") {
      std::string blob =
          "ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab"
          "😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃a"
          "b😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃"
          "ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab😃ab"
          "😃ab😃ab😃ab😃ab😃ab";

      SharedMemoryWriteStream write${"blobDataSizePipe", 65535, true};
        SharedMemoryReadStream read${"blobDataSizePipe", 65535, true};

        write$.write(blob);

        std::string dataString = read$.readString();

        EXPECT(blob == dataString);

        std::cout << "5. std::string blob: SUCCESS" << std::endl;
    },

    CASE("Can read flags, sets the right datatype and data change bit flips") {

        SharedMemoryWriteStream write${"blobDataSizePipe2", 65535, true};
        SharedMemoryReadStream read${"blobDataSizePipe2", 65535, true};

        write$.write("foo!");

        char flagsData = read$.readFlags();

        EXPECT(read$.readLength(kMemoryTypeString) == 4);

        std::bitset<8> flags(flagsData);

        EXPECT(!!(flagsData & kMemoryTypeString));

        std::cout << "6. status flag shows string data type flag: SUCCESS: 0b"
                  << flags << std::endl;
        
        EXPECT(!!(flagsData & kMemoryChanged));

        std::cout << "6.1 status flag has the change bit set: SUCCESS: 0b"
                  << flags << std::endl;

        write$.write("foo!");

        char flagsData2 = read$.readFlags();
        std::bitset<8> flags2(flagsData2);

        EXPECT(!!(flagsData2 & ~kMemoryChanged));

        write$.write("foo!1");

        char flagsData3 = read$.readFlags();
        std::bitset<8> flags3(flagsData3);
        EXPECT(!!(flagsData3 & kMemoryChanged));

        std::cout
            << "6.2 status bit flips to zero when writing again: SUCCESS: 0b"
            << flags2 << std::endl;
        
        std::cout
            << "6.3 status bit flips to one when writing again: SUCCESS: 0b"
            << flags3 << std::endl;
        
        write$.close();
        read$.close();
    },

    CASE("Can write and read a float* array") {

      float numbers[72] = {
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

      EXPECT(read$.readLength(kMemoryTypeFloat) == 72);
      
      char flagsData = read$.readFlags();
      std::bitset<8> flags(flagsData);
    
      std::cout
          << "Flags for float* read: 0b"
          << flags << std::endl;
      EXPECT(!!(flagsData & kMemoryTypeFloat));
      EXPECT(!!(flagsData & kMemoryChanged));
      
      float* numbersReadPtr = read$.readFloatArray();

      EXPECT(numbers[0] == numbersReadPtr[0]);
      EXPECT(numbers[1] == numbersReadPtr[1]);
      EXPECT(numbers[2] == numbersReadPtr[2]);
      EXPECT(numbers[3] == numbersReadPtr[3]);
      EXPECT(numbers[71] == numbersReadPtr[71]);

      std::cout << "7. float[72]: SUCCESS" << std::endl;
      
        write$.write(numbers, 72);

        char flagsData2 = read$.readFlags();
        std::bitset<8> flags2(flagsData2);

        EXPECT(!!(flagsData2 & ~kMemoryChanged));

        write$.write(numbers, 72);

        char flagsData3 = read$.readFlags();
        std::bitset<8> flags3(flagsData3);
        EXPECT(!!(flagsData3 & kMemoryChanged));

        std::cout
            << "7.1 status bit flips to zero when writing again: SUCCESS: 0b"
            << flags2 << std::endl;
        
        std::cout
            << "7.2 status bit flips to one when writing again: SUCCESS: 0b"
            << flags3 << std::endl;


      delete[] numbersReadPtr;
      write$.close();
      read$.close();
    },

    CASE("Can write and read a double* array") {

      double numbers[72] = {
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

      EXPECT(read$.readLength(kMemoryTypeDouble) == 72);

      char flagsData = read$.readFlags();
      std::bitset<8> flags(flagsData);
    
      std::cout
          << "Flags for double* read: 0b"
          << flags << std::endl;
      EXPECT(!!(flagsData & kMemoryTypeDouble));
      EXPECT(!!(flagsData & kMemoryChanged));

      double* numbersReadPtr = read$.readDoubleArray();

      EXPECT(numbers[0] == numbersReadPtr[0]);
      EXPECT(numbers[1] == numbersReadPtr[1]);
      EXPECT(numbers[2] == numbersReadPtr[2]);
      EXPECT(numbers[3] == numbersReadPtr[3]);
      EXPECT(numbers[71] == numbersReadPtr[71]);

      std::cout << "8. double[72]: SUCCESS" << std::endl;

        write$.write(numbers, 72);

        char flagsData2 = read$.readFlags();
        std::bitset<8> flags2(flagsData2);

        EXPECT(!!(flagsData2 & ~kMemoryChanged));

        write$.write(numbers, 72);

        char flagsData3 = read$.readFlags();
        std::bitset<8> flags3(flagsData3);
        EXPECT(!!(flagsData3 & kMemoryChanged));

        std::cout
            << "8.1 status bit flips to zero when writing again: SUCCESS: 0b"
            << flags2 << std::endl;
        
        std::cout
            << "8.2 status bit flips to one when writing again: SUCCESS: 0b"
            << flags3 << std::endl;
      delete[] numbersReadPtr;
      write$.close();
      read$.close();
    },
};

int main (int argc, char *argv[]) {
  return lest::run(specification, argc, argv);
}
