// Example: Stream-based shared memory transfer (C++20)
//
// Demonstrates SharedMemoryWriteStream / SharedMemoryReadStream for
// string and float-array IPC within a single process.

#include <libsharedmemory/libsharedmemory.hpp>
#include <iostream>
#include <vector>

int main()
{
    using namespace lsm;

    // --- String transfer ---
    std::string data = "{ foo: 'coolest IPC ever! 🧑‍💻' }";

    SharedMemoryWriteStream writer{"myChannel", 65535, true};
    SharedMemoryReadStream reader{"myChannel", 65535, true};

    writer.write(data);
    std::string result = reader.readString();

    std::cout << "String:  " << result << "\n";

    if (result != data) {
        std::cerr << "FAIL: string mismatch\n";
        return 1;
    }

    // --- Float array transfer ---
    std::vector<float> samples = {1.0f, 2.5f, 3.14f, -0.5f, 100.0f};
    writer.write(std::span<const float>(samples));

    float* out = reader.readFloatArray();
    std::cout << "Floats: ";
    for (size_t i = 0; i < samples.size(); ++i) {
        std::cout << out[i] << (i + 1 < samples.size() ? ", " : "\n");
        if (samples[i] != out[i]) {
            std::cerr << "FAIL: float mismatch at index " << i << "\n";
            delete[] out;
            return 1;
        }
    }
    delete[] out;

    writer.close();
    reader.close();
    writer.destroy();

    std::cout << "OK\n";
    return 0;
}
