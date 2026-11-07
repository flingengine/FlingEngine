#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

#include <array>
#include <cstddef>
#include <unordered_map>

namespace Fling
{
    /**
     * Blocks of power-of-two sizes from MinBlockBytes to MaxBlockBytes, cut from slabs
     * of OS pages. Each size has its own slabs and a free list, so a freed block is used
     * again for the same size and the memory does not fragment. A block is aligned to
     * its own size. A request bigger than MaxBlockBytes gets its own pages.
     *
     * Each slab is aligned to its own size, so the slab of a block is found from the
     * block's address with no search. Memory goes back to the OS only in Trim, which
     * frees the slabs that have no block in use, so the allocator does not take and
     * give back pages while a game runs. The slabs of the biggest blocks are one large
     * page each (see VirtualMemory::UseLargePages).
     *
     * Not thread safe: one owner (a world's storage) uses it.
     */
    class CORE_API BlockAllocator
    {
    public:
        static constexpr size_t MinBlockBytes = 1024;
        static constexpr size_t MaxBlockBytes = 128 * 1024;
        static constexpr size_t ClassCount = 8; // 1, 2, 4, ... 128 KiB

        struct Stats
        {
            /** Bytes taken from the OS: every slab, and every large block. */
            size_t ReservedBytes = 0;
            /** Bytes in blocks that are in use (whole blocks, so it includes their unused tail). */
            size_t UsedBytes = 0;
            size_t Slabs = 0;
            size_t LargeBlocks = 0;
        };

        BlockAllocator() = default;
        BlockAllocator(const BlockAllocator&) = delete;
        BlockAllocator& operator=(const BlockAllocator&) = delete;
        ~BlockAllocator();

        /** The size of the block that a request for bytes gets: the next power of two (at least MinBlockBytes), or
         * whole pages. */
        static size_t BlockSize(size_t bytes);

        /** The alignment of a block for bytes: the block size, or the page size for a large block. */
        static size_t BlockAlignment(size_t bytes);

        /** A block of BlockSize(bytes). Asserts and returns null if the OS has no memory. */
        void* Allocate(size_t bytes);

        /** Give back a block. Bytes is the value given to Allocate. */
        void Free(void* block, size_t bytes);

        /** Give the OS the slabs that have no block in use. Returns the bytes given back. */
        size_t Trim();

        Stats GetStats() const;

    private:
        struct FreeBlock
        {
            FreeBlock* Next;
        };

        struct SizeClass
        {
            FreeBlock* FreeList = nullptr;
            /** Slab address -> blocks in use */
            std::unordered_map<uintptr_t, uint32> Slabs;
            size_t UsedBlocks = 0;
        };

        static size_t ClassOf(size_t bytes);
        static size_t ClassBlockBytes(size_t sizeClass) { return MinBlockBytes << sizeClass; }
        /** A slab holds at least 8 blocks, and is at least 64 KiB (the Windows allocation granularity). */
        static size_t ClassSlabBytes(size_t sizeClass);

        bool AddSlab(size_t sizeClass);

        std::array<SizeClass, ClassCount> m_Classes{};

        /** Large block address -> bytes */
        std::unordered_map<uintptr_t, size_t> m_Large;
        size_t m_LargeBytes = 0;
    };
} // namespace Fling
