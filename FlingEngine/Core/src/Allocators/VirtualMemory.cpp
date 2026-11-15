#include "Allocators/VirtualMemory.h"
#include "Logger.h"
#include "MathUtils.h"

#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <limits>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace Fling::VirtualMemory
{
    namespace
    {
        /** True if bytes rounded up to pages, plus alignment, fits in size_t. */
        bool SizeFits(const size_t bytes, const size_t alignment)
        {
            const size_t limit = std::numeric_limits<size_t>::max() - PageSize();
            return bytes <= limit && alignment <= limit - bytes;
        }
    } // namespace

    void OutOfMemory(const char* who, const size_t bytes)
    {
        F_LOG_ERROR("{}: the OS has no memory for {} bytes. The program stops.", who, bytes);
        std::abort();
    }

    size_t PageSize()
    {
#if defined(_WIN32)
        static const size_t s_Size = []
        {
            SYSTEM_INFO info{};
            GetSystemInfo(&info);
            return static_cast<size_t>(info.dwPageSize);
        }();
#else
        static const size_t s_Size = static_cast<size_t>(sysconf(_SC_PAGESIZE));
#endif
        return s_Size;
    }

#if defined(_WIN32)
    namespace
    {
        /** VirtualAlloc places blocks on its allocation granularity (64 KiB). */
        size_t Granularity()
        {
            static const size_t s_Granularity = []
            {
                SYSTEM_INFO info{};
                GetSystemInfo(&info);
                return static_cast<size_t>(info.dwAllocationGranularity);
            }();
            return s_Granularity;
        }
    } // namespace

    void* Allocate(const size_t bytes, const size_t alignment)
    {
        assert(Math::IsPowerOfTwo(alignment));
        if (!SizeFits(bytes, alignment))
        {
            return nullptr;
        }
        const size_t size = Math::AlignUp(bytes, PageSize());
        if (alignment <= Granularity())
        {
            return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }

        // Windows cannot free part of a reservation. Reserve more address space than needed, so
        // that an aligned block is inside it, and commit only that block. The rest of the
        // reservation stays reserved until Free: it takes address space and no memory. No other
        // thread can take the address, so this needs no retry. Free finds the start of the
        // reservation again with VirtualQuery.
        void* reserved = VirtualAlloc(nullptr, size + alignment, MEM_RESERVE, PAGE_NOACCESS);
        if (reserved == nullptr)
        {
            return nullptr;
        }
        const uintptr_t aligned = Math::AlignUp<uintptr_t>(reinterpret_cast<uintptr_t>(reserved), alignment);
        void* result = VirtualAlloc(reinterpret_cast<void*>(aligned), size, MEM_COMMIT, PAGE_READWRITE);
        if (result == nullptr)
        {
            VirtualFree(reserved, 0, MEM_RELEASE);
        }
        return result;
    }

    void UseLargePages(void* address, const size_t bytes)
    {
        (void)address;
        (void)bytes;
    }

    void Free(void* address, const size_t bytes, const size_t alignment)
    {
        (void)bytes;
        (void)alignment;
        if (address == nullptr)
        {
            return;
        }
        // MEM_RELEASE takes the start of the reservation, which is before the block when it was aligned
        MEMORY_BASIC_INFORMATION info{};
        const SIZE_T found = VirtualQuery(address, &info, sizeof(info));
        assert(found != 0 && "VirtualMemory::Free: not memory from VirtualMemory::Allocate");
        void* base = found != 0 ? info.AllocationBase : address;
        [[maybe_unused]] const BOOL released = VirtualFree(base, 0, MEM_RELEASE);
        assert(released && "VirtualMemory::Free: VirtualFree failed");
    }
#else
    void* Allocate(const size_t bytes, const size_t alignment)
    {
        assert(Math::IsPowerOfTwo(alignment));
        if (!SizeFits(bytes, alignment))
        {
            return nullptr;
        }
        const size_t size = Math::AlignUp(bytes, PageSize());
        if (alignment <= PageSize())
        {
            void* result = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            return result == MAP_FAILED ? nullptr : result;
        }

        // Map more than needed, then unmap the parts before and after the aligned block
        const size_t mapped = size + alignment;
        void* raw = mmap(nullptr, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (raw == MAP_FAILED)
        {
            return nullptr;
        }
        const uintptr_t start = reinterpret_cast<uintptr_t>(raw);
        const uintptr_t aligned = Math::AlignUp<uintptr_t>(start, alignment);
        if (aligned > start)
        {
            munmap(raw, aligned - start);
        }
        const uintptr_t end = start + mapped;
        if (end > aligned + size)
        {
            munmap(reinterpret_cast<void*>(aligned + size), end - (aligned + size));
        }
        return reinterpret_cast<void*>(aligned);
    }

    void UseLargePages(void* address, const size_t bytes)
    {
#if defined(MADV_HUGEPAGE)
        if (address != nullptr && bytes >= LargePageBytes)
        {
            // Only a hint: if the kernel has no large page free, it uses small pages
            madvise(address, bytes, MADV_HUGEPAGE);
        }
#else
        (void)address;
        (void)bytes;
#endif
    }

    void Free(void* address, const size_t bytes, const size_t alignment)
    {
        (void)alignment;
        if (address != nullptr)
        {
            [[maybe_unused]] const int result = munmap(address, Math::AlignUp(bytes, PageSize()));
            assert(result == 0 && "VirtualMemory::Free: not memory from VirtualMemory::Allocate, or the wrong size");
        }
    }
#endif
} // namespace Fling::VirtualMemory
