#include "Allocators/BlockAllocator.h"
#include "Allocators/VirtualMemory.h"
#include "MathUtils.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <vector>

namespace Fling
{
    BlockAllocator::~BlockAllocator()
    {
        for (size_t c = 0; c < ClassCount; ++c)
        {
            const size_t slabBytes = ClassSlabBytes(c);
            for (const auto& [address, used] : m_Classes[c].Slabs)
            {
                (void)used;
                VirtualMemory::Free(reinterpret_cast<void*>(address), slabBytes, slabBytes);
            }
        }
        for (const auto& [address, bytes] : m_Large)
        {
            VirtualMemory::Free(reinterpret_cast<void*>(address), bytes, VirtualMemory::PageSize());
        }
    }

    size_t BlockAllocator::ClassOf(const size_t bytes)
    {
        // Blocks are MinBlockBytes << class
        size_t sizeClass = 0;
        for (size_t size = Math::NextPowerOfTwo(Math::Max(bytes, MinBlockBytes)); size > MinBlockBytes; size >>= 1)
        {
            ++sizeClass;
        }
        return sizeClass;
    }

    size_t BlockAllocator::ClassSlabBytes(const size_t sizeClass)
    {
        // The biggest blocks hold the chunks of big archetypes. Their slabs are one large
        // page, so filling them takes one page fault, and iterating them few TLB entries.
        if (ClassBlockBytes(sizeClass) == MaxBlockBytes)
        {
            return VirtualMemory::LargePageBytes;
        }
        return Math::Max<size_t>(64 * 1024, ClassBlockBytes(sizeClass) * 8);
    }

    size_t BlockAllocator::BlockSize(const size_t bytes)
    {
        if (bytes > MaxBlockBytes)
        {
            return Math::AlignUp(bytes, VirtualMemory::PageSize());
        }
        return ClassBlockBytes(ClassOf(bytes));
    }

    size_t BlockAllocator::BlockAlignment(const size_t bytes)
    {
        return bytes > MaxBlockBytes ? VirtualMemory::PageSize() : BlockSize(bytes);
    }

    bool BlockAllocator::AddSlab(const size_t sizeClass)
    {
        const size_t slabBytes = ClassSlabBytes(sizeClass);
        const size_t blockBytes = ClassBlockBytes(sizeClass);
        std::byte* slab = static_cast<std::byte*>(VirtualMemory::Allocate(slabBytes, slabBytes));
        if (slab == nullptr)
        {
            return false;
        }
        VirtualMemory::UseLargePages(slab, slabBytes);

        SizeClass& sc = m_Classes[sizeClass];
        sc.Slabs.emplace(reinterpret_cast<uintptr_t>(slab), 0u);
        // Link the blocks in address order, so blocks are handed out front to back
        for (size_t offset = slabBytes; offset >= blockBytes; offset -= blockBytes)
        {
            FreeBlock* block = reinterpret_cast<FreeBlock*>(slab + offset - blockBytes);
            block->Next = sc.FreeList;
            sc.FreeList = block;
        }
        return true;
    }

    void* BlockAllocator::Allocate(const size_t bytes)
    {
        if (bytes > MaxBlockBytes)
        {
            const size_t size = BlockSize(bytes);
            void* block = VirtualMemory::Allocate(size, VirtualMemory::PageSize());
            assert(block != nullptr && "BlockAllocator: the OS has no memory left");
            if (block != nullptr)
            {
                m_Large.emplace(reinterpret_cast<uintptr_t>(block), size);
                m_LargeBytes += size;
            }
            return block;
        }

        const size_t sizeClass = ClassOf(bytes);
        SizeClass& sc = m_Classes[sizeClass];
        if (sc.FreeList == nullptr && !AddSlab(sizeClass))
        {
            assert(false && "BlockAllocator: the OS has no memory left");
            return nullptr;
        }

        FreeBlock* block = sc.FreeList;
        sc.FreeList = block->Next;
        const size_t slabBytes = ClassSlabBytes(sizeClass);
        ++sc.Slabs.find(reinterpret_cast<uintptr_t>(block) & ~(slabBytes - 1))->second;
        ++sc.UsedBlocks;
        return block;
    }

    void BlockAllocator::Free(void* block, const size_t bytes)
    {
        if (block == nullptr)
        {
            return;
        }
        if (bytes > MaxBlockBytes)
        {
            const auto found = m_Large.find(reinterpret_cast<uintptr_t>(block));
            assert(found != m_Large.end() && "BlockAllocator::Free: not a large block of this allocator");
            VirtualMemory::Free(block, found->second, VirtualMemory::PageSize());
            m_LargeBytes -= found->second;
            m_Large.erase(found);
            return;
        }

        const size_t sizeClass = ClassOf(bytes);
        SizeClass& sc = m_Classes[sizeClass];
        const size_t slabBytes = ClassSlabBytes(sizeClass);
        const auto slab = sc.Slabs.find(reinterpret_cast<uintptr_t>(block) & ~(slabBytes - 1));
        assert(slab != sc.Slabs.end() && slab->second > 0 && "BlockAllocator::Free: not a block of this allocator");
        --slab->second;
        --sc.UsedBlocks;

        FreeBlock* freed = static_cast<FreeBlock*>(block);
        freed->Next = sc.FreeList;
        sc.FreeList = freed;
    }

    size_t BlockAllocator::Trim()
    {
        size_t released = 0;
        for (size_t c = 0; c < ClassCount; ++c)
        {
            SizeClass& sc = m_Classes[c];
            const size_t slabBytes = ClassSlabBytes(c);
            std::vector<uintptr_t> empty;
            for (const auto& [address, used] : sc.Slabs)
            {
                if (used == 0)
                {
                    empty.push_back(address);
                }
            }
            if (empty.empty())
            {
                continue;
            }
            std::sort(empty.begin(), empty.end());

            // Take the blocks of the empty slabs out of the free list, then give the slabs back
            FreeBlock** link = &sc.FreeList;
            while (*link != nullptr)
            {
                const uintptr_t slab = reinterpret_cast<uintptr_t>(*link) & ~(slabBytes - 1);
                if (std::binary_search(empty.begin(), empty.end(), slab))
                {
                    *link = (*link)->Next;
                }
                else
                {
                    link = &(*link)->Next;
                }
            }
            for (const uintptr_t address : empty)
            {
                sc.Slabs.erase(address);
                VirtualMemory::Free(reinterpret_cast<void*>(address), slabBytes, slabBytes);
                released += slabBytes;
            }
        }
        return released;
    }

    BlockAllocator::Stats BlockAllocator::GetStats() const
    {
        Stats stats;
        for (size_t c = 0; c < ClassCount; ++c)
        {
            stats.ReservedBytes += m_Classes[c].Slabs.size() * ClassSlabBytes(c);
            stats.UsedBytes += m_Classes[c].UsedBlocks * ClassBlockBytes(c);
            stats.Slabs += m_Classes[c].Slabs.size();
        }
        stats.ReservedBytes += m_LargeBytes;
        stats.UsedBytes += m_LargeBytes;
        stats.LargeBlocks = m_Large.size();
        return stats;
    }
} // namespace Fling
