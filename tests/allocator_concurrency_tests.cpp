#include "comet/allocator.hpp"

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <thread>
#include <vector>

int main()
{
    constexpr std::size_t thread_count = 8;
    constexpr std::size_t iterations = 1000;
    constexpr std::size_t batch_size = 16;

    comet::AllocatorConfig config;
    config.pool_size = 8 * 1024 * 1024;
    comet::Allocator allocator(config);
    std::atomic<bool> test_failed(false);
    std::vector<std::thread> threads;

    for (std::size_t t = 0; t < thread_count; t++)
    {
        threads.emplace_back([&, t]()
                             {
            for(std::size_t iteration = 0;
                iteration < iterations;
                ++iteration){
                    std::vector<void*> allocations;
                    allocations.reserve(batch_size);

                    // allocate a batch of differently sized blocks
                    for(std::size_t i = 0; i<batch_size; i++){
                        const std::size_t size = 1 + ((t*37 + iterations * 13 + i*7) % 256);

                        void *ptr = allocator.allocate(size);
                        if(ptr == nullptr){
                            test_failed.store(true);
                            break;
                        }

                        auto* bytes = static_cast<std::uint8_t*> (ptr);

                        // verifying the allocation mem is writable
                        bytes[0] = static_cast<std::uint8_t> (t);
                        bytes[size - 1] = static_cast<std::uint8_t> (iteration);
                        allocations.push_back(ptr);
                    }

                    // release everything allocated in this batch
                    for(void * ptr : allocations){
                        if(!allocator.deallocate(ptr)){
                            test_failed.store(true);
                        }
                    }

                    if(test_failed.load()) return;
                } });
    }

    for (auto &thread : threads)
        thread.join();

    assert(!test_failed.load());

    const auto stats = allocator.stats();
    constexpr std::size_t expected_allocations = thread_count * iterations * batch_size;

    std::cout << "Threads: " << thread_count << "\n";
    std::cout << "Expected allocations: " << expected_allocations << '\n';
    std::cout << "Actual allocations: " << stats.total_allocations << '\n';
    std::cout << "Total deallocations: " << stats.total_deallocations << '\n';
    std::cout << "Live allocations: " << stats.live_allocations << '\n';
    std::cout << "Failed allocations: " << stats.failed_allocations << '\n';

    assert(stats.total_allocations == expected_allocations);
    assert(stats.total_deallocations == expected_allocations);
    assert(stats.live_allocations == 0);
    assert(stats.allocated_bytes == 0);
    assert(stats.failed_allocations == 0);

    std::cout << "\nAll concurrency tests passed.\n";
    return 0;
}