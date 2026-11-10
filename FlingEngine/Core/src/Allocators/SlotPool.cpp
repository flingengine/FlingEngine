#include "Allocators/MemoryPoison.h"
#include "Allocators/SlotPool.h"
#include "MathUtils.h"

#include <algorithm>
#include <cassert>

namespace Fling
{
    SlotPool::SlotPool(BlockAllocator& blocks, const size_t slotBytes, const size_t alignment) : m_Blocks(blocks)
    {
        const size_t align = Math::Max(alignment, alignof(FreeSlot));
        // A free slot holds the free list link, so a slot is at least a pointer
        m_Stride = Math::AlignUp(Math::Max(slotBytes, sizeof(FreeSlot)), align);
        // A page is aligned to its size, so slots at multiples of the stride are aligned too
        m_PageBytes = BlockAllocator::BlockSize(Math::Max(PageBytes, m_Stride, align));
        assert(BlockAllocator::BlockAlignment(m_PageBytes) >= align && "SlotPool: alignment is more than a page");
    }

    SlotPool::~SlotPool()
    {
        for (void* page : m_Pages)
        {
            FLING_UNPOISON_MEMORY(page, m_PageBytes);
            m_Blocks.Free(page, m_PageBytes);
        }
    }

    void SlotPool::AddPage()
    {
        std::byte* page = static_cast<std::byte*>(m_Blocks.Allocate(m_PageBytes));
        m_Pages.push_back(page);
        const size_t slots = m_PageBytes / m_Stride;
        for (size_t i = slots; i > 0; --i)
        {
            FreeSlot* slot = reinterpret_cast<FreeSlot*>(page + (i - 1) * m_Stride);
            slot->Next = m_FreeList;
            m_FreeList = slot;
            // A free slot keeps only its link. The rest is not in use.
            FLING_POISON_MEMORY(reinterpret_cast<std::byte*>(slot) + sizeof(FreeSlot), m_Stride - sizeof(FreeSlot));
        }
    }

    void* SlotPool::Allocate()
    {
        if (m_FreeList == nullptr)
        {
            AddPage();
        }
        FreeSlot* slot = m_FreeList;
        m_FreeList = slot->Next;
        ++m_Used;
        FLING_UNPOISON_MEMORY(slot, m_Stride);
        return slot;
    }

    void SlotPool::Free(void* slot)
    {
        if (slot == nullptr)
        {
            return;
        }
        FreeSlot* freed = static_cast<FreeSlot*>(slot);
        freed->Next = m_FreeList;
        m_FreeList = freed;
        --m_Used;
        // A use of the slot after this is a use after free
        FLING_POISON_MEMORY(static_cast<std::byte*>(slot) + sizeof(FreeSlot), m_Stride - sizeof(FreeSlot));
    }
} // namespace Fling
