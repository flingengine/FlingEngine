#include "FreeList.h"

namespace Fling
{
    FreeList::FreeList(void* t_Start, void* t_End, size_t t_ElmSize, size_t alignment, size_t offset)
    {
        union
        {
            void* as_void;
            char* as_char;
            FreeList* as_self;
        };

        as_void = t_Start;

        // assume as_self points to the first entry in the free list
        m_Next = as_self;
        as_char += t_ElmSize;

        // initialize the free list - make every m_next of each element point to the next element in the list
        FreeList* runner = m_Next;

		// Calculate the number of elements will fit in this buffer
		ptrdiff_t bufferSize = ((const char*)t_End - (const char*)t_Start);
		assert(bufferSize > 0);
		size_t NumElements = bufferSize / t_ElmSize;

        for (size_t i = 1; i < NumElements; ++i)
        {
            runner->m_Next = as_self;
            runner = as_self;
            as_char += t_ElmSize;
        }
        
        runner->m_Next = nullptr;
    }
}