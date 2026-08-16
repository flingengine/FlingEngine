#pragma once

#include <stdlib.h>     // size_t
#include <cstddef>		// std::ptrdiff_t
#include <assert.h>

#include "Memory.h"

namespace Fling
{
    /**
     * Stack (LIFO) allocator over a fixed memory region.
     *
     * @see https://blog.molecular-matters.com/2012/08/27/memory-allocation-strategies-a-stack-like-lifo-allocator/
     */
    class StackAllocator
    {
    public:
        /**
         * Create a stack allocator over the memory between t_Start and t_End.
         *
         * @param t_Start  Start of the memory block to use for this stack allocator
         * @param t_End    End of the memory block to use for this stack allocator
         */
        StackAllocator(void* t_Start, void* t_End);
        ~StackAllocator();

        /**
         * Allocate a block from the top of the stack.
         *
         * @param t_Size        Size of the block of memory
         * @param t_Alignment   Alignment of the element (Default = 8)
         * @param t_Offset      Offset of the element (Default = 0)
         * @return Pointer to the allocated block (asserts when we exceed the preallocated size)
         */
        void* Allocate(size_t t_Size, size_t t_Alignment = 0, size_t t_Offset = 0);

        /**
         * Return a block of memory to the stack in LIFO order.
         *
         * @param t_Ptr  Pointer previously returned by Allocate
         */
        void Free(void* t_Ptr);

    private:
        char* m_Start = nullptr;
        char* m_End = nullptr;
        char* m_Current = nullptr;
    };
}
