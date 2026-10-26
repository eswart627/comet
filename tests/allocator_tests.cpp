#include "comet/allocator.hpp"

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

int main()
{
    std::cout << "1. Test started." << std::endl;

    comet::Allocator allocator;
    std::cout << "2. Allocator constructed." << std::endl;

    void *ptr = allocator.allocate(100);
    std::cout << "3. Allocation returned: " << ptr << std::endl;

    assert(ptr != nullptr);

    auto address = reinterpret_cast<std::uintptr_t>(ptr);
    assert(address % alignof(std::max_align_t) == 0);
    std::cout << "4. Alignment check passed." << std::endl;

    auto *bytes = static_cast<std::uint8_t *>(ptr);
    bytes[0] = 42;
    bytes[99] = 99;

    assert(bytes[0] == 42);
    assert(bytes[99] == 99);
    std::cout << "5. Memory read/write test passed." << std::endl;

    const auto stats = allocator.stats();
    std::cout << "Expected pool size: "
              << 64ULL * 1024 * 1024 << " bytes\n";
    std::cout << "Reported free bytes: "
              << stats.free_bytes << " bytes\n";
    std::cout << "6. Statistics retrieved." << std::endl;
    std::cout << "Pool size: " << stats.pool_size << " bytes\n";
    std::cout << "Allocated bytes: " << stats.allocated_bytes << " bytes\n";
    std::cout << "Free bytes: " << stats.free_bytes << " bytes\n";
    std::cout << "Live allocations: " << stats.live_allocations << '\n';
    std::cout << "Total allocations: " << stats.total_allocations << '\n';

    assert(stats.live_allocations == 1);
    assert(stats.total_allocations == 1);
    assert(stats.allocated_bytes == 100);

    bool freed = allocator.deallocate(ptr);
    assert(freed);

    auto stats_after_free = allocator.stats();

    assert(stats_after_free.live_allocations == 0);
    assert(stats_after_free.allocated_bytes == 0);
    assert(stats_after_free.total_deallocations == 1);

    std::cout << "Deallocation test passed." << std::endl;

    std::cout << "\nAll tests passed" << std::endl;

    return 0;
}