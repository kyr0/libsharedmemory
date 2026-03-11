// Example: FIFO message queue (C++20)
//
// Demonstrates SharedMemoryQueue: enqueue from a writer, dequeue and
// peek from a reader within a single process.

#include <libsharedmemory/libsharedmemory.hpp>
#include <iostream>

int main()
{
    using namespace lsm;

    SharedMemoryQueue writer{"queue", 10, 256, true, true};
    SharedMemoryQueue reader{"queue", 10, 256, true, false};

    writer.enqueue("First message");
    writer.enqueue("Second message");

    std::string msg;

    if (!reader.dequeue(msg) || msg != "First message") {
        std::cerr << "FAIL: expected 'First message', got '" << msg << "'\n";
        return 1;
    }
    std::cout << "Dequeued: " << msg << "\n";

    if (!reader.peek(msg) || msg != "Second message") {
        std::cerr << "FAIL: expected peek 'Second message', got '" << msg << "'\n";
        return 1;
    }
    std::cout << "Peeked:   " << msg << "\n";

    std::cout << "Size: " << reader.size()
              << ", Empty: " << reader.isEmpty() << "\n";

    writer.close();
    reader.close();
    writer.destroy();

    std::cout << "OK\n";
    return 0;
}
