#include "comet/allocator.hpp"

#include <cassert>
#include <cstddef>
#include <iostream>

int main()
{
    // Keep the pool small so every scenario is easy to reason about.
    constexpr std::size_t pool_size = 4096;
    comet::AllocatorConfig config;
    config.pool_size = pool_size;

    comet::Allocator allocator(config);

    const auto initial = allocator.stats();

    std::cout << "Initial state:\n"
              << "  pool_size   = " << initial.pool_size << '\n'
              << "  free_bytes  = " << initial.free_bytes << '\n';

    assert(initial.pool_size == pool_size);
    assert(initial.allocated_bytes == 0);
    assert(initial.live_allocations == 0);
    assert(initial.free_bytes > 0);
    const std::size_t initial_free = initial.free_bytes;

    // Test 1: One allocation must reduce free_bytes; freeing it must
    // restore the exact initial value.
    void* a = allocator.allocate(100);
    assert(a != nullptr);

    const auto after_one_alloc = allocator.stats();
    std::cout << "\nAfter allocate(100):\n"
              << "  allocated_bytes = " << after_one_alloc.allocated_bytes << '\n'
              << "  free_bytes      = " << after_one_alloc.free_bytes << '\n'
              << "  free-space drop = "
              << (initial_free - after_one_alloc.free_bytes) << '\n';

    assert(after_one_alloc.allocated_bytes == 100);
    assert(after_one_alloc.live_allocations == 1);
    assert(after_one_alloc.free_bytes < initial_free);

    assert(allocator.deallocate(a));
    const auto after_one_free = allocator.stats();

    std::cout << "\nAfter deallocate(a):\n"
              << "  allocated_bytes = " << after_one_free.allocated_bytes << '\n'
              << "  free_bytes      = " << after_one_free.free_bytes << '\n';

    assert(after_one_free.allocated_bytes == 0);
    assert(after_one_free.live_allocations == 0);
    assert(after_one_free.free_bytes == initial_free);

    // Test 2: Several allocations, then free them in an order that exercises
    // coalescing with both next and previous free blocks.
    void* b = allocator.allocate(128);
    void* c = allocator.allocate(256);
    void* d = allocator.allocate(64);
    assert(b && c && d);

    const auto after_three_allocs = allocator.stats();
    std::cout << "\nAfter allocate(128), allocate(256), allocate(64):\n"
              << "  allocated_bytes = " << after_three_allocs.allocated_bytes << '\n'
              << "  free_bytes      = " << after_three_allocs.free_bytes << '\n';

    assert(after_three_allocs.allocated_bytes == 448);
    assert(after_three_allocs.live_allocations == 3);
    assert(after_three_allocs.free_bytes < initial_free);

    // Free the middle allocation first, then the first and last allocations.
    assert(allocator.deallocate(c));
    const auto after_free_middle = allocator.stats();
    std::cout << "\nAfter deallocate(c):\n"
              << "  allocated_bytes = " << after_free_middle.allocated_bytes << '\n'
              << "  free_bytes      = " << after_free_middle.free_bytes << '\n';

    assert(after_free_middle.allocated_bytes == 192);
    assert(after_free_middle.live_allocations == 2);

    assert(allocator.deallocate(b));
    const auto after_free_first = allocator.stats();
    std::cout << "\nAfter deallocate(b):\n"
              << "  allocated_bytes = " << after_free_first.allocated_bytes << '\n'
              << "  free_bytes      = " << after_free_first.free_bytes << '\n';

    assert(after_free_first.allocated_bytes == 64);
    assert(after_free_first.live_allocations == 1);

    assert(allocator.deallocate(d));
    const auto final_stats = allocator.stats();

    std::cout << "\nAfter deallocate(d), all blocks coalesced:\n"
              << "  allocated_bytes = " << final_stats.allocated_bytes << '\n'
              << "  free_bytes      = " << final_stats.free_bytes << '\n'
              << "  initial free    = " << initial_free << '\n';

    assert(final_stats.allocated_bytes == 0);
    assert(final_stats.live_allocations == 0);
    assert(final_stats.free_bytes == initial_free);

    std::cout << "\nAll free-space accounting tests passed.\n";
    return 0;
}
