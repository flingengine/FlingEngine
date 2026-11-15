#pragma once

#include "Allocators/BlockAllocator.h"
#include "CoreAPI.h"
#include "FlingTypes.h"

#include <cstddef>
#include <vector>

namespace Fling
{
    /**
     * Slots of one size and alignment, in pages from a BlockAllocator. A slot keeps its
     * address until it is freed, and a freed slot is used again first, so a pool of many
     * small objects that come and go does not fragment memory or call the heap. The
     * objects in the slots are not constructed or destroyed here. Not thread safe.
     */
    class CORE_API SlotPool
    {
    public:
        static constexpr size_t PageBytes = 16 * 1024;

        /** The allocator must live longer than the pool. */
        SlotPool(BlockAllocator& blocks, size_t slotBytes, size_t alignment);
        SlotPool(const SlotPool&) = delete;
        SlotPool& operator=(const SlotPool&) = delete;
        ~SlotPool();

        /** Raw memory for one object. Never null. */
        void* Allocate();

        void Free(void* slot);

        size_t SlotBytes() const { return m_Stride; }

        size_t UsedSlots() const { return m_Used; }

    private:
        struct FreeSlot
        {
            FreeSlot* Next;
        };

        void AddPage();

        BlockAllocator& m_Blocks;
        size_t m_Stride = 0;
        size_t m_PageBytes = 0;
        FreeSlot* m_FreeList = nullptr;
        std::vector<void*> m_Pages;
        size_t m_Used = 0;
    };
} // namespace Fling
