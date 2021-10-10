//#include <chrono>
#include <libsharedmemory/libsharedmemory.hpp>
#include "lest.hpp"
#include <iostream>
//#include <thread>
//#include <random>

using namespace std;
using namespace lsm;

// random device and generator
//std::random_device _rd;
//std::mt19937 _gen(_rd());

const lest::test specification[] = {
    CASE("shared memory can be created and opened and transfer uint8_t") {
        Memory memoryWriter {"lsmtest", 64, true};
        EXPECT(kOK == memoryWriter.create());

        memoryWriter.data()[0] = 0x11;
        memoryWriter.data()[1] = 0x34;

        Memory memoryReader{"lsmtest", 64, true};

        EXPECT(kOK == memoryReader.open());

        std::cout << "1. single uint8_t: SUCCESS" << std::endl;

        EXPECT(0x11 == memoryReader.data()[0]);
        EXPECT(0x34 == memoryReader.data()[1]);
    },

    CASE("non-existing shared memory objects err") {
        Memory memoryReader{"lsmtest2", 64, true};
        EXPECT(kErrorOpeningFailed == memoryReader.open());
        std::cout << "2. error when opening non-existing segment: SUCCESS" << std::endl;
    },

    CASE("using MemoryStreamWriter and MemoryStreamReader to transfer "
        "std::string") {

        std::string dataToTransfer = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

        SharedMemoryWriteStream write${"jsonPipe", 65535, true};
        SharedMemoryReadStream read${"jsonPipe", 65535, true};

        write$.write(dataToTransfer);

        std::string dataString = read$.read();

        std::cout << "3. std::string (UTF8): SUCCESS | " << dataString << std::endl;

        EXPECT(dataToTransfer == dataString);
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

            std::string dataString = read$.read();

            EXPECT(t2 == dataString);
        }
        std::cout << "4. std::string more/less: SUCCESS; 1000 runs" << std::endl;
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

        std::string dataString = read$.read();

        EXPECT(blob == dataString);

        std::cout << "5. std::string blob: SUCCESS" << std::endl;
    }

    /*

    CASE("using onChange to listen for changes in shared memory") {

       std::string dataToTransfer = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

        SharedMemoryWriteStream write${"jsonPipe2", 65535, true};
        SharedMemoryReadStream read${"jsonPipe2", 65535, true};

        // test sync write early (buffer filled)
        write$.write(dataToTransfer);

        read$.onChange([](std::string &dataChanged) {
            std::cout << "lambda, dataChanged " << dataChanged << std::endl;
        });


        // test sync write late
        write$.write("test1");

        std::thread writeSimulator;

        // test async writing
        writeSimulator = std::thread([&] {

          // random between 5ms and 200ms distribution
           std::uniform_int_distribution<> randomDistribution(5, 200);

          for (int i = 0; i < 20; i++) {
            write$.write("changedData");
            std::this_thread::sleep_for(std::chrono::milliseconds(randomDistribution(_gen)));
          }
        });
        writeSimulator.join();
    },
    */
};

int main (int argc, char *argv[]) {
  return lest::run(specification, argc, argv);
}
