#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

#include <cstddef>
#include <vector>

namespace Fling
{
    /**
     * A bump allocator for memory that all dies at once, such as one tick's deferred
     * changes. Allocate moves a pointer; Reset makes all of it free again. Objects in
     * the arena are not destroyed: the owner runs destructors before Reset.
     *
     * Its pages come from the OS and stay after Reset, so once the arena has grown to
     * the most that a tick needs, it takes no more memory. Not thread safe.
     */
    class CORE_API LinearArena
    {
    public:
        static constexpr size_t PageBytes = 64 * 1024;

        LinearArena() = default;
        LinearArena(const LinearArena&) = delete;
        LinearArena& operator=(const LinearArena&) = delete;
        LinearArena(LinearArena&& other) noexcept;
        LinearArena& operator=(LinearArena&& other) noexcept;
        ~LinearArena();

        /** Memory for bytes at a multiple of alignment (a power of two). Never null. */
        void* Allocate(size_t bytes, size_t alignment);

        /** Make every allocation free again. The pages stay. */
        void Reset();

        /** Give back every page. */
        void Release();

        /** Bytes taken from the OS. */
        size_t ReservedBytes() const;

        /** Bytes handed out since the last Reset (with alignment padding). */
        size_t UsedBytes() const { return m_Used; }

    private:
        struct Page
        {
            std::byte* Memory = nullptr;
            size_t Bytes = 0;
        };

        std::vector<Page> m_Pages;
        size_t m_Current = 0;
        size_t m_Offset = 0;
        size_t m_Used = 0;
    };
} // namespace Fling
