#include <libsharedmemory/libsharedmemory.hpp>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

using namespace lsm;

namespace {

struct BenchResult {
    std::string name;
    int threads;
    std::uint64_t operations;
    double seconds;

    [[nodiscard]] double opsPerSec() const {
        return seconds > 0.0 ? static_cast<double>(operations) / seconds : 0.0;
    }
};

class StartGate {
public:
    explicit StartGate(const int target) : _target(target) {}

    void arriveAndWait() {
        _ready.fetch_add(1, std::memory_order_release);
        while (!_go.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }

    void releaseAll() {
        while (_ready.load(std::memory_order_acquire) < _target) {
            std::this_thread::yield();
        }
        _go.store(true, std::memory_order_release);
    }

private:
    int _target;
    std::atomic<int> _ready{0};
    std::atomic<bool> _go{false};
};

BenchResult benchStreamWriters(const int writerThreads, const int opsPerThread) {
    const std::string shmName = "bench_stream_contention";
    SharedMemoryWriteStream writer{shmName, 1024, true};

    StartGate gate(writerThreads);
    std::vector<std::thread> threads;
    threads.reserve(static_cast<std::size_t>(writerThreads));

    const auto t0 = std::chrono::steady_clock::now();

    for (int tid = 0; tid < writerThreads; ++tid) {
        threads.emplace_back([&, tid]() {
            gate.arriveAndWait();
            for (int i = 0; i < opsPerThread; ++i) {
                writer.write("S-" + std::to_string(tid) + "-" + std::to_string(i));
            }
        });
    }

    gate.releaseAll();

    for (auto &th : threads) {
        th.join();
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(t1 - t0).count();

    writer.close();
    writer.destroy();

    return {"stream_writers", writerThreads, static_cast<std::uint64_t>(writerThreads) * opsPerThread, seconds};
}

BenchResult benchQueueProducers(const int producerThreads, const int msgsPerProducer) {
    const std::string qName = "bench_queue_producers";
    SharedMemoryQueue writer{qName, 4096, 64, true, true};
    SharedMemoryQueue reader{qName, 4096, 64, true, false};

    const std::uint64_t total = static_cast<std::uint64_t>(producerThreads) * msgsPerProducer;
    std::atomic<std::uint64_t> consumed{0};

    StartGate gate(producerThreads + 1);

    std::thread consumer([&]() {
        gate.arriveAndWait();
        std::string msg;
        while (consumed.load(std::memory_order_acquire) < total) {
            if (reader.dequeue(msg)) {
                consumed.fetch_add(1, std::memory_order_release);
            } else {
                std::this_thread::yield();
            }
        }
    });

    std::vector<std::thread> producers;
    producers.reserve(static_cast<std::size_t>(producerThreads));

    const auto t0 = std::chrono::steady_clock::now();

    for (int tid = 0; tid < producerThreads; ++tid) {
        producers.emplace_back([&, tid]() {
            gate.arriveAndWait();
            for (int i = 0; i < msgsPerProducer; ++i) {
                const std::string msg = "P-" + std::to_string(tid) + "-" + std::to_string(i);
                while (!writer.enqueue(msg)) {
                    std::this_thread::yield();
                }
            }
        });
    }

    gate.releaseAll();

    for (auto &th : producers) {
        th.join();
    }
    consumer.join();

    const auto t1 = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(t1 - t0).count();

    writer.close();
    reader.close();
    writer.destroy();

    return {"queue_producers", producerThreads, total, seconds};
}

BenchResult benchQueueConsumers(const int consumerThreads, const int msgsPerConsumer) {
    const std::string qName = "bench_queue_consumers";
    SharedMemoryQueue writer{qName, 4096, 64, true, true};

    const std::uint64_t total = static_cast<std::uint64_t>(consumerThreads) * msgsPerConsumer;

    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<bool> producerDone{false};

    StartGate gate(consumerThreads + 1);

    std::thread producer([&]() {
        gate.arriveAndWait();
        for (std::uint64_t i = 0; i < total; ++i) {
            const std::string msg = "C-" + std::to_string(i);
            while (!writer.enqueue(msg)) {
                std::this_thread::yield();
            }
            produced.fetch_add(1, std::memory_order_release);
        }
        producerDone.store(true, std::memory_order_release);
    });

    std::vector<std::thread> consumers;
    consumers.reserve(static_cast<std::size_t>(consumerThreads));

    const auto t0 = std::chrono::steady_clock::now();

    for (int i = 0; i < consumerThreads; ++i) {
        consumers.emplace_back([&]() {
            SharedMemoryQueue reader{qName, 4096, 64, true, false};
            gate.arriveAndWait();
            std::string msg;
            while (true) {
                if (reader.dequeue(msg)) {
                    consumed.fetch_add(1, std::memory_order_release);
                } else if (producerDone.load(std::memory_order_acquire)
                           && consumed.load(std::memory_order_acquire) >= total) {
                    break;
                } else {
                    std::this_thread::yield();
                }
            }
            reader.close();
        });
    }

    gate.releaseAll();

    producer.join();
    for (auto &th : consumers) {
        th.join();
    }

    const auto t1 = std::chrono::steady_clock::now();
    const double seconds = std::chrono::duration<double>(t1 - t0).count();

    writer.close();
    writer.destroy();

    return {"queue_consumers", consumerThreads, consumed.load(std::memory_order_acquire), seconds};
}

void printResult(const BenchResult &r, const double baselineOpsPerSec) {
    const double current = r.opsPerSec();
    const double dropPct = baselineOpsPerSec > 0.0 ? (1.0 - (current / baselineOpsPerSec)) * 100.0 : 0.0;

    std::cout << std::left << std::setw(18) << r.name
              << " threads=" << std::setw(2) << r.threads
              << " ops=" << std::setw(8) << r.operations
              << " time=" << std::fixed << std::setprecision(3) << std::setw(8) << r.seconds
              << " ops/s=" << std::fixed << std::setprecision(1) << std::setw(12) << current
              << " drop_vs_1t=" << std::fixed << std::setprecision(1) << dropPct << "%"
              << std::endl;
}

} // namespace

int main() {
    std::cout << "libsharedmemory contention benchmark" << std::endl;
    std::cout << "------------------------------------" << std::endl;

    const std::vector<int> threadCounts = {1, 2, 4, 8};

    // Stream writer contention
    std::vector<BenchResult> streamResults;
    for (const int t : threadCounts) {
        streamResults.push_back(benchStreamWriters(t, 200000));
    }
    const double streamBaseline = streamResults.front().opsPerSec();
    for (const auto &r : streamResults) {
        printResult(r, streamBaseline);
    }

    std::cout << std::endl;

    // Queue producer contention (single consumer)
    std::vector<BenchResult> producerResults;
    for (const int t : threadCounts) {
        producerResults.push_back(benchQueueProducers(t, 100000));
    }
    const double producerBaseline = producerResults.front().opsPerSec();
    for (const auto &r : producerResults) {
        printResult(r, producerBaseline);
    }

    std::cout << std::endl;

    // Queue consumer contention (single producer)
    std::vector<BenchResult> consumerResults;
    for (const int t : threadCounts) {
        consumerResults.push_back(benchQueueConsumers(t, 100000));
    }
    const double consumerBaseline = consumerResults.front().opsPerSec();
    for (const auto &r : consumerResults) {
        printResult(r, consumerBaseline);
    }
    return 0;
}
