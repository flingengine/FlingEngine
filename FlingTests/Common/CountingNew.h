#pragma once

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

// A counting global operator new for allocation tests. It replaces the global one for the
// whole test executable, so only one source file of an executable includes this header.
// It counts (g_Allocations) only while a test turns it on (g_CountAllocations).
//
// ThreadSanitizer has its own global operator new, and two definitions do not link.
// A TSan build leaves the counter out (FLING_TEST_COUNT_ALLOCATIONS is 0), and the tests skip.

#if defined(__SANITIZE_THREAD__)
#define FLING_TEST_COUNT_ALLOCATIONS 0
#elif defined(__has_feature)
#if __has_feature(thread_sanitizer)
#define FLING_TEST_COUNT_ALLOCATIONS 0
#endif
#endif
#ifndef FLING_TEST_COUNT_ALLOCATIONS
#define FLING_TEST_COUNT_ALLOCATIONS 1
#endif

#if FLING_TEST_COUNT_ALLOCATIONS
namespace
{
    std::atomic<bool> g_CountAllocations{false};
    std::atomic<size_t> g_Allocations{0};

    void* CountedAllocate(const size_t bytes)
    {
        if (g_CountAllocations.load(std::memory_order_relaxed))
        {
            g_Allocations.fetch_add(1, std::memory_order_relaxed);
        }
        if (void* memory = std::malloc(bytes == 0 ? 1 : bytes))
        {
            return memory;
        }
        throw std::bad_alloc();
    }

    void* CountedAllocateAligned(const size_t bytes, const std::align_val_t alignment)
    {
        if (g_CountAllocations.load(std::memory_order_relaxed))
        {
            g_Allocations.fetch_add(1, std::memory_order_relaxed);
        }
        const size_t align = static_cast<size_t>(alignment);
        const size_t rounded = (bytes + align - 1) / align * align;
#if defined(_WIN32)
        void* memory = _aligned_malloc(rounded == 0 ? align : rounded, align);
#else
        void* memory = std::aligned_alloc(align, rounded == 0 ? align : rounded);
#endif
        if (memory == nullptr)
        {
            throw std::bad_alloc();
        }
        return memory;
    }

    void FreeAligned(void* memory)
    {
#if defined(_WIN32)
        _aligned_free(memory);
#else
        std::free(memory);
#endif
    }
} // namespace

void* operator new(size_t bytes)
{
    return CountedAllocate(bytes);
}
void* operator new[](size_t bytes)
{
    return CountedAllocate(bytes);
}
void* operator new(size_t bytes, std::align_val_t alignment)
{
    return CountedAllocateAligned(bytes, alignment);
}
void* operator new[](size_t bytes, std::align_val_t alignment)
{
    return CountedAllocateAligned(bytes, alignment);
}
void* operator new(size_t bytes, const std::nothrow_t&) noexcept
{
    try
    {
        return CountedAllocate(bytes);
    }
    catch (...)
    {
        return nullptr;
    }
}
void* operator new[](size_t bytes, const std::nothrow_t& tag) noexcept
{
    return operator new(bytes, tag);
}
void* operator new(size_t bytes, std::align_val_t alignment, const std::nothrow_t&) noexcept
{
    try
    {
        return CountedAllocateAligned(bytes, alignment);
    }
    catch (...)
    {
        return nullptr;
    }
}
void* operator new[](size_t bytes, std::align_val_t alignment, const std::nothrow_t& tag) noexcept
{
    return operator new(bytes, alignment, tag);
}
void operator delete(void* memory) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, size_t) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory, size_t) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::align_val_t) noexcept
{
    FreeAligned(memory);
}
void operator delete[](void* memory, std::align_val_t) noexcept
{
    FreeAligned(memory);
}
void operator delete(void* memory, size_t, std::align_val_t) noexcept
{
    FreeAligned(memory);
}
void operator delete[](void* memory, size_t, std::align_val_t) noexcept
{
    FreeAligned(memory);
}
void operator delete(void* memory, const std::nothrow_t&) noexcept
{
    std::free(memory);
}
void operator delete[](void* memory, const std::nothrow_t&) noexcept
{
    std::free(memory);
}
void operator delete(void* memory, std::align_val_t, const std::nothrow_t&) noexcept
{
    FreeAligned(memory);
}
void operator delete[](void* memory, std::align_val_t, const std::nothrow_t&) noexcept
{
    FreeAligned(memory);
}
#else
namespace
{
    std::atomic<bool> g_CountAllocations{false};
    std::atomic<size_t> g_Allocations{0};
} // namespace
#endif
