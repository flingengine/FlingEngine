#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

#include <cstddef>

namespace Fling::VirtualMemory
{
    /** The OS page size (4 KiB on most systems). */
    CORE_API size_t PageSize();

    /**
     * Pages straight from the OS (mmap or VirtualAlloc), readable and writable, and
     * zero until written. Bytes is rounded up to whole pages. Alignment is a power of
     * two; the block starts at a multiple of it. Pages that are never written take
     * no physical memory on Linux or Windows.
     * @return Null if the OS has no memory left
     */
    CORE_API void* Allocate(size_t bytes, size_t alignment);

    /** Give pages from Allocate back to the OS. Bytes and alignment are the values given to Allocate. */
    CORE_API void Free(void* address, size_t bytes, size_t alignment);
} // namespace Fling::VirtualMemory
