#include "StackAllocator.h"
#include "FlingTypes.h"

namespace {
    static const size_t SIZE_OF_ALLOCATION_OFFSET = sizeof(uint32);
    static_assert(SIZE_OF_ALLOCATION_OFFSET == 4, "Allocation offset has wrong size");

    // posix_memalign rejects an alignment smaller than sizeof(void*).
    static const size_t STACK_BASE_ALIGNMENT = sizeof(void*);
    static_assert(STACK_BASE_ALIGNMENT >= SIZE_OF_ALLOCATION_OFFSET, "Stack base alignment is too small");
    static_assert((STACK_BASE_ALIGNMENT & (STACK_BASE_ALIGNMENT - 1)) == 0,
                  "Stack base alignment must be a power of two");
} // namespace

Fling::StackAllocator::StackAllocator(void* t_Start, void* t_End) {
    std::ptrdiff_t bufferSize = ((const char*)t_End - (const char*)t_Start);

    m_Start = static_cast<char*>(AlignedAlloc(static_cast<size_t>(bufferSize), STACK_BASE_ALIGNMENT));
    assert(m_Start != nullptr);
    if (!m_Start) {
        m_End = nullptr;
        m_Current = nullptr;
        return;
    }

    m_End = m_Start + bufferSize;
    m_Current = m_Start;
}

Fling::StackAllocator::~StackAllocator() {
    AlignedFree(m_Start);
}

void* Fling::StackAllocator::Allocate(size_t t_Size, size_t t_Alignment, size_t t_Offset) {
    if (!m_Start) {
        return nullptr;
    }

    if (t_Alignment == 0) {
        t_Alignment = sizeof(void*);
    }

    t_Size += SIZE_OF_ALLOCATION_OFFSET;
    t_Offset += SIZE_OF_ALLOCATION_OFFSET;

    const uint32 allocationOffset = static_cast<uint32>(m_Current - m_Start);

    // Align the user pointer. The 4-byte header stays in front of that pointer.
    char* aligned = AlignPointer<char>(m_Current + t_Offset, t_Alignment) - t_Offset;

    if (aligned < m_Start || aligned + t_Size > m_End) {
        assert(false && "Stack Allocator out of memory");
        return nullptr;
    }

    m_Current = aligned;

    union {
        void* as_void;
        char* as_char;
        uint32* as_uint32_t;
    };

    as_char = m_Current;

    // store allocation offset in the first 4 bytes
    *as_uint32_t = allocationOffset;
    as_char += SIZE_OF_ALLOCATION_OFFSET;

    void* userPtr = as_void;
    m_Current += t_Size;
    return userPtr;
}

void Fling::StackAllocator::Free(void* t_Ptr) {
    assert(t_Ptr);

    union {
        void* as_void;
        char* as_char;
        uint32* as_uint32_t;
    };

    as_void = t_Ptr;

    // grab the allocation offset from the 4 bytes right before the given pointer
    as_char -= SIZE_OF_ALLOCATION_OFFSET;
    const uint32 allocationOffset = *as_uint32_t;

    m_Current = m_Start + allocationOffset;
}
