#ifndef COMET_ALLOCATOR_HPP
#define COMET_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <limits>

namespace comet
{
    struct AllocatorConfig
    {
        std::size_t pool_size = 64 * 1024 * 1024;
    };

    struct AllocatorStats
    {
        std::size_t pool_size = 0;
        std::size_t allocated_bytes = 0;
        std::size_t free_bytes = 0;

        std::size_t live_allocations = 0;
        std::uint64_t total_allocations = 0;
        std::uint64_t total_deallocations = 0;
        std::uint64_t failed_allocations = 0;
    };

    class Allocator
    {
    public:
        explicit Allocator(const AllocatorConfig &config = {});

        ~Allocator();

        // diable copying: allocator owns unique mem pool
        Allocator(const Allocator &) = delete;
        Allocator &operator=(Allocator &&) = delete;

        // returns nullptr for zero-size alloc failues
        void *allocate(
            std::size_t size,
            std::size_t alignment = alignof(std::max_align_t));

        // returns true if the ptr was successfully deallocated
        // nullptr is accepted as no-op
        bool deallocate(void *ptr) noexcept;

        // returns a consistent snapshot of allcator stats;
        AllocatorStats stats() const;

    private:
        struct Block;
        AllocatorConfig config_;

        // implementations will be in src/allocator.cpp
    };
} // namespace comet

#endif