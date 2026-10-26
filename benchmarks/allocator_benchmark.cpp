#include "comet/allocator.hpp"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <vector>

using Clock = std::chrono::steady_clock;

constexpr std::size_t iterations = 5000;
constexpr std::size_t allocation_size = 64;

template <typename Allocate, typename Deallocate>
double benchmark(Allocate allocate, Deallocate deallocate)
{
    std::vector<void *> pointers;
    pointers.reserve(iterations);

    const auto start = Clock::now();

    for (std::size_t i = 0; i < iterations; ++i)
    {
        void *ptr = allocate(allocation_size);

        if (ptr == nullptr)
        {
            std::cerr << "Allocation failed at iteration "
                      << i << '\n';

            for (void *p : pointers)
                deallocate(p);

            return -1.0;
        }

        pointers.push_back(ptr);
    }

    for (void *ptr : pointers)
        deallocate(ptr);

    const auto end = Clock::now();

    return std::chrono::duration<double, std::milli>(
               end - start)
        .count();
}

int main()
{
    comet::AllocatorConfig config;
    config.pool_size = 64 * 1024 * 1024;

    comet::Allocator comet_allocator(config);

    const double comet_ms = benchmark(
        [&](std::size_t size)
        {
            return comet_allocator.allocate(size);
        },
        [&](void *ptr)
        {
            comet_allocator.deallocate(ptr);
        });

    const double system_ms = benchmark(
        [](std::size_t size)
        {
            return std::malloc(size);
        },
        [](void *ptr)
        {
            std::free(ptr);
        });

    if (comet_ms < 0.0 || system_ms < 0.0)
        return 1;

    std::cout << "Iterations: " << iterations << '\n';
    std::cout << "Allocation size: "
              << allocation_size << " bytes\n\n";

    std::cout << "Comet:  " << comet_ms << " ms\n";
    std::cout << "System: " << system_ms << " ms\n";

    std::cout << "Comet throughput: "
              << (2.0 * iterations / (comet_ms / 1000.0))
              << " operations/sec\n";

    std::cout << "System throughput: "
              << (2.0 * iterations / (system_ms / 1000.0))
              << " operations/sec\n";

    return 0;
}