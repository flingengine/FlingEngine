#include "StackAllocator.h"
#include "FlingTypes.h"

namespace 
{
    static const size_t SIZE_OF_ALLOCATION_OFFSET = sizeof(UINT32);
    static_assert(SIZE_OF_ALLOCATION_OFFSET == 4, "Allocation offset has wrong size");
}


Fling::StackAllocator::StackAllocator(void* t_Start, void* t_End)
{
    std::ptrdiff_t bufferSize = ((const char*)t_End - (const char*)t_Start);

    m_Start = static_cast<char*>(AlignedAlloc(bufferSize, 0));
    m_End = m_Start + bufferSize;
}

void* Fling::StackAllocator::Allocate(size_t t_Size, size_t t_Alignment, size_t t_Offset)
{
    t_Size += SIZE_OF_ALLOCATION_OFFSET;
    t_Offset += SIZE_OF_ALLOCATION_OFFSET;

    const UINT32 allocationOffset = static_cast<UINT32>(m_Current - m_Start);
    
    //offset the pointer first, align it, and then offset it back
    m_Current = AlignPointer<char>(m_Current + t_Offset, t_Alignment) - t_Offset;

    if (m_Current + t_Size > m_End)
    {
        assert("Stack Allocator out of memory");
    }

    union
    {
        void* as_void;
        char* as_char;
        uint32_t* as_uint32_t;
    };

    as_char = m_Current;

    // store allocation offset in the first 4 bytes
    *as_uint32_t = allocationOffset;
    as_char += SIZE_OF_ALLOCATION_OFFSET;

    void* userPtr = as_void;
    m_Current += t_Size;
    return userPtr;
}

void Fling::StackAllocator::Free(void* t_Ptr)
{
    assert(t_Ptr);

    union
    {
        void* as_void;
        char* as_char;
        uint32_t* as_uint32_t;
    };

    as_void = t_Ptr;

    // grab the allocation offset from the 4 bytes right before the given pointer
    as_char -= SIZE_OF_ALLOCATION_OFFSET;
    const uint32_t allocationOffset = *as_uint32_t;

    m_Current = m_Start + allocationOffset;
}
