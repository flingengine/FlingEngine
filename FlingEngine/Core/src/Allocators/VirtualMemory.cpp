#include "Allocators/VirtualMemory.h"
#include "MathUtils.h"

#include <cassert>
#include <cstdint>

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
        const size_t size = Math::AlignUp(bytes, PageSize());
        if (alignment <= Granularity())
        {
            return VirtualAlloc(nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
        }

        // Windows cannot free part of a reservation: reserve more than needed to find an
        // aligned address, give it all back, then take exactly the aligned part. Another
        // thread can take the address in between, so try again a few times.
        for (int attempt = 0; attempt < 16; ++attempt)
        {
            void* probe = VirtualAlloc(nullptr, size + alignment, MEM_RESERVE, PAGE_NOACCESS);
            if (probe == nullptr)
            {
                return nullptr;
            }
            const uintptr_t aligned = Math::AlignUp<uintptr_t>(reinterpret_cast<uintptr_t>(probe), alignment);
            VirtualFree(probe, 0, MEM_RELEASE);
            void* result =
                VirtualAlloc(reinterpret_cast<void*>(aligned), size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
            if (result != nullptr)
            {
                return result;
            }
        }
        return nullptr;
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
        if (address != nullptr)
        {
            VirtualFree(address, 0, MEM_RELEASE);
        }
    }
#else
    void* Allocate(const size_t bytes, const size_t alignment)
    {
        assert(Math::IsPowerOfTwo(alignment));
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
            munmap(address, Math::AlignUp(bytes, PageSize()));
        }
    }
#endif
} // namespace Fling::VirtualMemory
