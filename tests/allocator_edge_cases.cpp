#include "comet/allocator.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>
using namespace std;

int main()
{
    comet::AllocatorConfig config;
    config.pool_size = 1024;
    comet::Allocator allocator(config);

    // Test 1: Zero-size allocation must fail
    assert(allocator.allocate(0) == nullptr);
    cout << "Test 1 passed: zero-size alloc rejected.\n";

    // Test 2: Invalid alignment (not a power of two).
    assert(allocator.allocate(32, 3) == nullptr);
    std::cout << "Test 2 passed: invalid alignment rejected.\n";

    // Test 3: Allocation larger than the pool must fail.
    assert(allocator.allocate(2048) == nullptr);
    std::cout << "Test 3 passed: oversized allocation rejected.\n";

    // Test 4: Pool exhaustion.
    void *ptr = allocator.allocate(800);
    assert(ptr != nullptr);

    void *ptr2 = allocator.allocate(800);
    assert(ptr2 == nullptr);

    std::cout << "Test 4 passed: pool exhaustion handled.\n";

    // Test 5: Failed allocations must not create live allocations.
    const auto stats = allocator.stats();
    assert(stats.live_allocations == 1);
    assert(stats.total_allocations == 1);
    assert(stats.failed_allocations == 4);
    std::cout << "Test 5 passed: statistics remain consistent.\n";

    std::cout << "\nAll edge-case tests passed.\n";
    return 0;
}