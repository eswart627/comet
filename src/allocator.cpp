#include "comet/allocator.hpp"

#include <stdexcept>
#include <cstdint>
#include <limits>
#include <iostream>

namespace comet
{
    struct Allocator::Block
    {
        std::size_t span;           // total physical size of this block
        std::size_t requested_size; // bytes requested by the caller
        std::size_t payload_offset; // distance from header to returned pointer

        bool free;

        Block *prev;
        Block *next;
    };

    std::size_t Allocator::align_up(std::size_t value) noexcept
    {
        const std::size_t remainder = value % block_alignment;

        if (remainder == 0)
            return value;
        return value + (block_alignment - remainder);
    }

    std::size_t Allocator::aligned_header_size() noexcept
    {
        return align_up(sizeof(Block));
    }

    std::uintptr_t align_address(
        std::uintptr_t address,
        std::size_t alignment) noexcept
    {
        const std::size_t remainder = address % alignment;
        if (remainder == 0)
            return address;
        const std::size_t padding = alignment - remainder;
        if (address > std::numeric_limits<std::uintptr_t>::max() - padding)
        {
            return 0;
        }
        return address + padding;
    }

    bool valid_alignment(std::size_t alignment) noexcept
    {
        return alignment != 0 &&
               (alignment & (alignment - 1)) == 0;
    }

    Allocator::Allocator(const AllocatorConfig &config) : config_(config)
    {
        const std::size_t header_size = aligned_header_size();

        if (config_.pool_size <= header_size)
        {
            throw std::invalid_argument("Pool size must be greater thatn block header");
        }

        pool_ = static_cast<std::byte *>(::operator new(config_.pool_size));

        // placement new: takes address as parameter and creates new objs there
        // placement new uses existing storage
        head_ = ::new (static_cast<void *>(pool_)) Block{
            config_.pool_size,
            0,
            0,
            true,
            nullptr,
            nullptr};

        stats_.pool_size = config_.pool_size;
        stats_.free_bytes = config_.pool_size - header_size;
    }

    Allocator::~Allocator()
    {
        ::operator delete(pool_);
    }

    void *Allocator::allocate(std::size_t size, std::size_t alignment)
    {
        std::lock_guard<std::mutex> lock(mutex_);

        // check whether the caller alignment request is valid or not
        if (size == 0 || !valid_alignment(alignment))
        {
            ++stats_.failed_allocations;
            return nullptr;
        }

        // ensure ordinary types are supported even when the caller
        //    requests a smaller alignment
        if (alignment < block_alignment)
        {
            alignment = block_alignment;
        }

        const std::size_t header_size = aligned_header_size();

        Block *current = head_;

        while (current != nullptr)
        {
            if (!current->free)
            {
                current = current->next;
                continue;
            }

            const auto block_start = reinterpret_cast<std::uintptr_t>(current);
            const auto block_end = block_start + current->span;

            const auto payload_address = align_address(
                block_start + header_size,
                alignment);

            // check whether this block can accomodate the request
            if (payload_address == 0 || payload_address > block_end ||
                size > block_end - payload_address)
            {
                current = current->next;
                continue;
            }

            // optimization commit: recording the free space in candidate block
            //      just chosen for allocation
            const std::size_t old_free_capacity = current->span - header_size;

            const auto request_end = payload_address + size;

            // place the next block header at an aligned address
            const auto split_address = align_address(
                request_end,
                block_alignment);

            // split only if the remainder can hold a header and
            //       atlease one byte of payload
            const bool can_split =
                split_address != 0 &&
                split_address <= block_end &&
                block_end - split_address > header_size;
            if (can_split)
            {
                Block *old_next = current->next;

                Block *remainder = ::new (
                    reinterpret_cast<void *>(split_address)) Block{
                    static_cast<std::size_t>(block_end - split_address),
                    0,
                    0,
                    true,
                    current,
                    old_next};
                if (old_next != nullptr)
                {
                    old_next->prev = remainder;
                }

                current->next = remainder;
                current->span = static_cast<std::size_t>(split_address - block_start);
            }

            current->free = false;
            current->requested_size = size;
            current->payload_offset =
                static_cast<std::size_t>(payload_address - block_start);

            ++stats_.live_allocations;
            ++stats_.total_allocations;
            stats_.allocated_bytes += size;

            // optimization commit: calculating any residual free space
            //      after possible split occurs
            std::size_t new_free_capacity = 0;
            if (can_split)
            {
                new_free_capacity = current->next->span - header_size;
            }
            // total - allocated block free space
            // then add residual free space
            // total + new_free_space
            stats_.free_bytes -= old_free_capacity;
            stats_.free_bytes += new_free_capacity;
            // re calculate the free payload capacity
            // stats_.free_bytes = 0;
            // for (Block *block = head_; block != nullptr; block = block->next)
            // {
            //     if (block->free && block->span > header_size)
            //     {
            //         stats_.free_bytes += block->span + header_size;
            //     }
            // }
            return reinterpret_cast<void *>(payload_address);
        }
        ++stats_.failed_allocations;
        return nullptr;
    }

    bool Allocator::deallocate(void *ptr) noexcept
    {
        if (ptr == nullptr)
            return true;

        std::lock_guard<std::mutex> lock(mutex_);

        // find the block whose payload starts with ptr
        Block *current = head_;
        while (current != nullptr)
        {
            auto *expected_ptr =
                reinterpret_cast<std::byte *>(current) + current->payload_offset;

            if (!current->free && expected_ptr == ptr)
                break;

            current = current->next;
        }

        // pointer doesnt match a live location
        if (current == nullptr)
            return false;

        // optimization commit: calculating free space out of coalesced block
        const std::size_t header_size = aligned_header_size();

        current->free = true;

        stats_.allocated_bytes -= current->requested_size;
        stats_.live_allocations--;
        ++stats_.total_deallocations;

        current->requested_size = 0;
        current->payload_offset = 0;

        // Add the newly freed block's usable capacity.
        stats_.free_bytes += current->span - header_size;

        // Merge with next free blocks.
        // Their payload capacities are already included in free_bytes.
        // Each merge removes one header, increasing usable capacity by header_size.
        while (current->next != nullptr && current->next->free)
        {
            Block *next = current->next;

            current->span += next->span;
            current->next = next->next;

            if (current->next != nullptr)
                current->next->prev = current;

            stats_.free_bytes += header_size;
        }

        // Merge with previous free blocks.
        while (current->prev != nullptr && current->prev->free)
        {
            Block *prev = current->prev;

            prev->span += current->span;
            prev->next = current->next;

            if (prev->next != nullptr)
                prev->next->prev = prev;

            current = prev;

            // Coalescing eliminates the current block's header.
            stats_.free_bytes += header_size;
        }

        return true;
    }

    AllocatorStats Allocator::stats() const
    {
        std::lock_guard<std::mutex> lock(mutex_);
        return stats_;
    }
} // namespace comet
