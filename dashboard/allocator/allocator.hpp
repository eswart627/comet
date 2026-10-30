#ifndef COMET_ALLOCATOR_HPP
#define COMET_ALLOCATOR_HPP

#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <vector>
namespace comet
{
    struct AllocatorConfig
    {
        std::size_t pool_size = 64 * 1024 * 1024; // 64 MiB
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
        Allocator &operator=(const Allocator &) = delete;
        Allocator(Allocator &&) = delete;
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

        // Dashboard-only diagnostics
        struct MemoryBlockInfo
        {
            std::size_t offset;
            std::size_t span;
            std::size_t requested_size;
            bool free;
        };

        std::vector<MemoryBlockInfo> memory_map() const;

    private:
        struct Block;
        AllocatorConfig config_;

        std::byte *pool_ = nullptr;
        Block *head_ = nullptr;

        mutable std::mutex mutex_;
        AllocatorStats stats_{};

        static constexpr std::size_t block_alignment = alignof(std::max_align_t);
        static std::size_t align_up(std::size_t value) noexcept;
        static std::size_t aligned_header_size() noexcept;
        // implementations will be in src/allocator.cpp
    };
} // namespace comet

#endif