#include "Allocators/LinearArena.h"
#include "Allocators/VirtualMemory.h"
#include "MathUtils.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
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
                return reinterpret_cast<void*>(at);
            }
            ++m_Current;
            m_Offset = 0;
        }

        // A new page, big enough for this request
        const size_t pageBytes = Math::Max(PageBytes, bytes + alignment);
        const size_t pageAlign = Math::Max(VirtualMemory::PageSize(), alignment);
        Page page{static_cast<std::byte*>(VirtualMemory::Allocate(pageBytes, pageAlign)), pageBytes};
        assert(page.Memory != nullptr && "LinearArena: the OS has no memory left");
        m_Pages.push_back(page);
        m_Current = m_Pages.size() - 1;
        m_Offset = 0;
        return Allocate(bytes, alignment);
    }

    void LinearArena::Reset()
    {
        m_Current = 0;
        m_Offset = 0;
        m_Used = 0;
    }

    void LinearArena::Release()
    {
        for (const Page& page : m_Pages)
        {
            VirtualMemory::Free(page.Memory, page.Bytes, VirtualMemory::PageSize());
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
