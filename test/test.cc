#include <libsharedmemory/libsharedmemory.hpp>
#include "lest.hpp"
#include <iostream>

using namespace std;
using namespace lsm;

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

    CASE("using MemoryStreamWriter and MemoryStreamReder to transfer "
        "std::string") {

        std::string dataToTransfer = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

        SharedMemoryWriteStream write${"jsonPipe", 65535, true};
        SharedMemoryReadStream read${"jsonPipe", 65535, true};

        write$.write(dataToTransfer);

        std::string dataString = read$.read();

        std::cout << "3. std::string (UTF8): SUCCESS | " << dataString << std::endl;

        EXPECT(dataToTransfer == dataString);      
    },
};

int main (int argc, char *argv[]) {
  return lest::run(specification, argc, argv);
}
