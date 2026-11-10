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
     * @return Null if the OS has no memory left, or if bytes plus alignment is too big for size_t
     */
    CORE_API void* Allocate(size_t bytes, size_t alignment);

    /** The size of a large page that UseLargePages asks for (2 MiB). */
    inline constexpr size_t LargePageBytes = 2 * 1024 * 1024;

    /**
     * Ask the OS to back pages from Allocate with large pages, where it can. One large
     * page takes one page fault and one TLB entry in place of 512. A hint only: Linux
     * uses it when transparent huge pages are on ("madvise" or "always"), and on Windows
     * it does nothing, because large pages there need a user right.
     */
    CORE_API void UseLargePages(void* address, size_t bytes);

    /** Give pages from Allocate back to the OS. Bytes and alignment are the values given to Allocate. */
    CORE_API void Free(void* address, size_t bytes, size_t alignment);

    /**
     * Stop the program because the OS has no memory for bytes: log who asked, then abort.
     * In every build. The engine allocators call this, so their callers never get a
     * null block and never write through one.
     */
    [[noreturn]] CORE_API void OutOfMemory(const char* who, size_t bytes);
} // namespace Fling::VirtualMemory
