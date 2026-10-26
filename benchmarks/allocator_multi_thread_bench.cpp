#include "comet/allocator.hpp"

#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>
#include <vector>

using Clock = std::chrono::steady_clock;

constexpr std::size_t iterations = 5000;
constexpr std::size_t allocation_size = 64;

template <typename Allocate, typename Deallocate>
double benchmark(
    std::size_t thread_count,
    Allocate allocate,
    Deallocate deallocate)
{
    std::atomic<bool> start_flag{false};
    std::atomic<bool> allocation_failed{false};

    std::vector<std::thread> workers;
    workers.reserve(thread_count);

    for (std::size_t t = 0; t < thread_count; ++t)
    {
        workers.emplace_back([&]()
                             {
            while (!start_flag.load(std::memory_order_acquire))
            {
                std::this_thread::yield();
            }

            for (std::size_t i = 0; i < iterations; ++i)
            {
                void* ptr = allocate(allocation_size);

                if (ptr == nullptr)
                {
                    allocation_failed.store(true);
                    return;
                }

                deallocate(ptr);
            } });
    }

    const auto begin = Clock::now();

    start_flag.store(true, std::memory_order_release);

    for (auto &worker : workers)
    {
        worker.join();
    }

    const auto end = Clock::now();

    if (allocation_failed.load())
    {
        return -1.0;
    }

    return std::chrono::duration<double, std::milli>(
               end - begin)
        .count();
}

int main()
{
    comet::AllocatorConfig config;
    config.pool_size = 64 * 1024 * 1024;

    comet::Allocator allocator(config);

    const std::vector<std::size_t> thread_counts = {1, 2, 4, 8};

    std::cout << "Concurrent Allocation Benchmark\n";
    std::cout << "Iterations per thread: " << iterations << '\n';
    std::cout << "Allocation size: " << allocation_size << " bytes\n";
    std::cout << "Each iteration: one allocation and one deallocation\n\n";

    std::cout << "Threads | Comet (ms) | System (ms)"
              << " | Comet (ops/sec) | System (ops/sec)\n";

    for (const std::size_t thread_count : thread_counts)
    {
        const double comet_ms = benchmark(
            thread_count,
            [&](std::size_t size)
            {
                return allocator.allocate(size);
            },
            [&](void *ptr)
            {
                allocator.deallocate(ptr);
            });

        const double system_ms = benchmark(
            thread_count,
            [](std::size_t size)
            {
                return std::malloc(size);
            },
            [](void *ptr)
            {
                std::free(ptr);
            });

        if (comet_ms < 0.0 || system_ms < 0.0)
        {
            std::cerr << "Benchmark failed with "
                      << thread_count << " threads.\n";
            return 1;
        }

        const double total_operations =
            2.0 * iterations * thread_count;

        const double comet_ops =
            total_operations / (comet_ms / 1000.0);

        const double system_ops =
            total_operations / (system_ms / 1000.0);

        std::cout << thread_count << "       | "
                  << comet_ms << "       | "
                  << system_ms << "       | "
                  << comet_ops << "       | "
                  << system_ops << '\n';
    }

    return 0;
}