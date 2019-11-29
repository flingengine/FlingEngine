#pragma once

#include <stdlib.h>     // size_t
#include <cstddef>		// std::ptrdiff_t
#include <assert.h>

#include "Memory.h"

namespace Fling
{
    class StackAllocator
    {
    public:
        StackAllocator(void* t_Start, void* t_End);
        ~StackAllocator();

        void* Allocate(size_t t_Size, size_t t_Alignment, size_t t_Offset);

        void Free(void* t_Ptr);

    private:
        char* m_Start = nullptr;
        char* m_End = nullptr;
        char* m_Current = nullptr;
    };
}