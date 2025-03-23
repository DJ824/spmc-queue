#include "spmc.hpp"
#include <atomic>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <thread>
#include <vector>

void pin_thread(int cpu) {
  if (cpu < 0) {
    return;
  }
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) ==
      -1) {
    perror("pthread_setaffinity_np");
    exit(1);
  }
}

template <typename T, size_t SIZE>
void consumer_thread(LockFreeSPMCQueue<T, SIZE> &queue,
                    std::atomic<bool> &producer_done,
                    std::atomic<size_t> &total_consumed, size_t expected_items,
                    int cpu_id) {
  if (cpu_id >= 0) {
    pin_thread(cpu_id);
  }

  auto reader = queue.create_reader();
  size_t count = 0;

  while (count < expected_items) {
    auto result = reader.read();
    if (result) {
      count++;
    } else if (producer_done.load(std::memory_order_acquire)) {
      break;
    }
  }

  total_consumed.fetch_add(count, std::memory_order_relaxed);
}

int main(int argc, char *argv[]) {
  int producer_cpu = -1;
  int base_consumer_cpu = -1;

  if (argc >= 3) {
    producer_cpu = std::stoi(argv[1]);
    base_consumer_cpu = std::stoi(argv[2]);
    std::cout << "Pinning producer to CPU " << producer_cpu
              << " and starting consumers from CPU " << base_consumer_cpu
              << std::endl;
  }

  const size_t QUEUE_SIZE = 1048576;
  const size_t NUM_ITERATIONS = 10000000;
  const int NUM_RUNS = 5;
  const std::vector<int> READER_COUNTS = {2, 4, 8};

  std::cout << "Queue capacity: " << QUEUE_SIZE << " elements" << std::endl;
  std::cout << "Operations per test: " << NUM_ITERATIONS << std::endl;
  std::cout << "Number of test runs: " << NUM_RUNS << std::endl << std::endl;

  for (int num_readers : READER_COUNTS) {
    std::cout << "======================================================="
              << std::endl;
    std::cout << "Single Producer, " << num_readers
              << " Consumers Throughput Test" << std::endl;
    std::cout << "======================================================="
              << std::endl;

    for (int run = 0; run < NUM_RUNS; ++run) {
      LockFreeSPMCQueue<int, QUEUE_SIZE> queue;
      std::atomic<bool> producer_done(false);
      std::atomic<size_t> total_consumed(0);

      std::vector<std::thread> consumers;
      consumers.reserve(num_readers);

      for (int i = 0; i < num_readers; ++i) {
        int cpu =
            (base_consumer_cpu >= 0)
                ? (base_consumer_cpu + i) % std::thread::hardware_concurrency()
                : -1;
        consumers.emplace_back(consumer_thread<int, QUEUE_SIZE>, std::ref(queue),
                               std::ref(producer_done),
                               std::ref(total_consumed), NUM_ITERATIONS, cpu);
      }

      if (producer_cpu >= 0) {
        pin_thread(producer_cpu);
      }

      auto start_time = std::chrono::high_resolution_clock::now();

      for (size_t i = 0; i < NUM_ITERATIONS; ++i) {
        while (!queue.enqueue(static_cast<int>(i))) {
          std::this_thread::yield();
        }
      }

      producer_done.store(true, std::memory_order_release);

      for (auto &consumer : consumers) {
        consumer.join();
      }

      auto end_time = std::chrono::high_resolution_clock::now();
      auto duration_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(
                             end_time - start_time)
                             .count();

      double throughput = (NUM_ITERATIONS * 1000000.0) / duration_ns;
      double latency = static_cast<double>(duration_ns) / NUM_ITERATIONS;

      size_t items_consumed = total_consumed.load();
      double consumption_rate =
          (static_cast<double>(items_consumed) / NUM_ITERATIONS) * 100.0;
      double avg_per_consumer =
          static_cast<double>(items_consumed) / num_readers;

      std::cout << "Run " << (run + 1) << ":" << std::endl;
      std::cout << "  Operations: " << NUM_ITERATIONS << std::endl;
      std::cout << "  Duration: " << std::fixed << std::setprecision(2)
                << (duration_ns / 1000000.0) << " ms" << std::endl;
      std::cout << "  Throughput: " << std::fixed << std::setprecision(2)
                << throughput << " ops/ms" << std::endl;
      std::cout << "  Latency: " << std::fixed << std::setprecision(2)
                << latency << " ns/op" << std::endl;
      std::cout << "  Total items consumed: " << items_consumed << " ("
                << std::fixed << std::setprecision(2) << consumption_rate
                << "%)" << std::endl;
      std::cout << "  Avg items per consumer: " << std::fixed
                << std::setprecision(2) << avg_per_consumer << std::endl;
      std::cout << std::endl;
    }
  }

  return 0;
}
