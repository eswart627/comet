#pragma once

#include <cstddef>
#include <vector>

namespace comet::diagnostics
{

    struct MemoryBlock
    {
        std::size_t offset = 0;
        std::size_t span = 0;
        std::size_t requested_size = 0;
        bool free = true;
    };

    struct MemorySnapshot
    {
        std::size_t pool_size = 0;
        std::vector<MemoryBlock> blocks;
    };

} // namespace comet::diagnostics