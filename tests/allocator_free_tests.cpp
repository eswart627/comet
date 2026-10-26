#include "comet/allocator.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>

int main()
{
    comet::AllocatorConfig config;
    config.pool_size = 4096;

    comet::Allocator allocator(config);

    // Test 1: Allocate and free a block.
    void *p = allocator.allocate(256);
    assert(p != nullptr);

    assert(allocator.deallocate(p));

    auto stats = allocator.stats();
    assert(stats.live_allocations == 0);
    assert(stats.allocated_bytes == 0);
    assert(stats.total_deallocations == 1);

    std::cout << "Test 1 passed: allocation and deallocation.\n";

    // Test 2: Double free must be rejected.
    assert(!allocator.deallocate(p));

    stats = allocator.stats();
    assert(stats.live_allocations == 0);
    assert(stats.total_deallocations == 1);

    std::cout << "Test 2 passed: double free rejected.\n";

    // Test 3: Freed memory can be reused.
    void *q = allocator.allocate(256);
    assert(q != nullptr);
    assert(p == q);
    assert(allocator.deallocate(q));

    std::cout << "Test 3 passed: freed memory reused.\n";

    // Test 4: Coalescing adjacent free blocks.
    comet::AllocatorConfig small_config;
    small_config.pool_size = 2048;

    comet::Allocator coalesce_allocator(small_config);

    void *a = coalesce_allocator.allocate(400);
    void *b = coalesce_allocator.allocate(400);
    void *c = coalesce_allocator.allocate(400);
    void *d = coalesce_allocator.allocate(400);

    assert(a != nullptr);
    assert(b != nullptr);
    assert(c != nullptr);
    assert(d != nullptr);

    // Consume any remaining space so it cannot satisfy the later request.
    void *filler = nullptr;
    while (void *p = coalesce_allocator.allocate(1))
    {
        filler = p;
    }

    // Free adjacent blocks A and B.
    assert(coalesce_allocator.deallocate(a));
    assert(coalesce_allocator.deallocate(b));

    // A 700-byte request cannot fit in either 400-byte block alone.
    // It should fit in the combined space after coalescing.
    void *merged = coalesce_allocator.allocate(700);
    assert(merged != nullptr);

    std::cout << "Test 4 passed: adjacent free blocks coalesced.\n";

    stats = allocator.stats();
    assert(stats.live_allocations == 0);
    assert(stats.allocated_bytes == 0);

    std::cout << "\nAll deallocation tests passed.\n";

    return 0;
}