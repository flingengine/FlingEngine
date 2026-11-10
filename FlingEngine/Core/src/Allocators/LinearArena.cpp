#include "Allocators/LinearArena.h"
#include "Allocators/MemoryPoison.h"
#include "Allocators/VirtualMemory.h"
#include "MathUtils.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>
#include <utility>

namespace Fling
{
    LinearArena::LinearArena(LinearArena&& other) noexcept
        : m_Pages(std::move(other.m_Pages)), m_Current(other.m_Current), m_Offset(other.m_Offset), m_Used(other.m_Used)
    {
        other.m_Pages.clear();
        other.m_Current = 0;
        other.m_Offset = 0;
        other.m_Used = 0;
    }

    LinearArena& LinearArena::operator=(LinearArena&& other) noexcept
    {
        if (this != &other)
        {
            Release();
            m_Pages = std::move(other.m_Pages);
            m_Current = other.m_Current;
            m_Offset = other.m_Offset;
            m_Used = other.m_Used;
            other.m_Pages.clear();
            other.m_Current = 0;
            other.m_Offset = 0;
            other.m_Used = 0;
        }
        return *this;
    }

    LinearArena::~LinearArena()
    {
        Release();
    }

    void* LinearArena::Allocate(const size_t bytes, const size_t alignment)
    {
        assert(Math::IsPowerOfTwo(alignment));
        // A size this big is a bug in the caller (a negative number as a size, for example). The sums
        // below would wrap around, so stop here.
        if (bytes > std::numeric_limits<size_t>::max() / 4 || alignment > std::numeric_limits<size_t>::max() / 4)
        {
            VirtualMemory::OutOfMemory("LinearArena", bytes);
        }
        // Try the current page, then each later page kept from earlier ticks
        while (m_Current < m_Pages.size())
        {
            Page& page = m_Pages[m_Current];
            const uintptr_t base = reinterpret_cast<uintptr_t>(page.Memory);
            const uintptr_t at = Math::AlignUp<uintptr_t>(base + m_Offset, alignment);
            if (at + bytes <= base + page.Bytes)
            {
                m_Used += (at + bytes) - (base + m_Offset);
                m_Offset = (at + bytes) - base;
                FLING_UNPOISON_MEMORY(reinterpret_cast<void*>(at), bytes);
                return reinterpret_cast<void*>(at);
            }
            ++m_Current;
            m_Offset = 0;
        }

        // A new page, big enough for this request
        const size_t pageBytes = Math::Max(PageBytes, bytes + alignment);
        const size_t pageAlign = Math::Max(VirtualMemory::PageSize(), alignment);
        Page page{static_cast<std::byte*>(VirtualMemory::Allocate(pageBytes, pageAlign)), pageBytes, pageAlign};
        if (page.Memory == nullptr)
        {
            VirtualMemory::OutOfMemory("LinearArena", pageBytes);
        }
        // Nothing in the page is in use until Allocate hands it out
        FLING_POISON_MEMORY(page.Memory, page.Bytes);
        m_Pages.push_back(page);
        m_Current = m_Pages.size() - 1;
        m_Offset = 0;
        return Allocate(bytes, alignment);
    }

    void LinearArena::Reset()
    {
        // A use of an old allocation after this is a use after free
        for (const Page& page : m_Pages)
        {
            FLING_POISON_MEMORY(page.Memory, page.Bytes);
        }
        m_Current = 0;
        m_Offset = 0;
        m_Used = 0;
    }

    void LinearArena::Release()
    {
        for (const Page& page : m_Pages)
        {
            FLING_UNPOISON_MEMORY(page.Memory, page.Bytes);
            VirtualMemory::Free(page.Memory, page.Bytes, page.Alignment);
        }
        m_Pages.clear();
        Reset();
    }

    size_t LinearArena::ReservedBytes() const
    {
        size_t total = 0;
        for (const Page& page : m_Pages)
        {
            total += page.Bytes;
        }
        return total;
    }
} // namespace Fling
