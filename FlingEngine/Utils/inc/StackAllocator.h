#pragma once

#include <stdlib.h>     // size_t
#include <cstddef>		// std::ptrdiff_t
#include <assert.h>

#include "Memory.h"

namespace Fling
{
    /**
     * @brief 
     * 
     * @see https://blog.molecular-matters.com/2012/08/27/memory-allocation-strategies-a-stack-like-lifo-allocator/
     */
    class StackAllocator
    {
    public:
        /**
         * @brief Construct a new Stack Allocator object
         * 
         * @param t_Start  Start of the memory block to use for this stack allocator
         * @param t_End    End of the memory block to use for this stack allocator 
         */
        StackAllocator(void* t_Start, void* t_End);
        ~StackAllocator();

        /**
         * @brief 
         * 
         * @param t_Size        Size of the block of memory 
         * @param t_Alignment   Alignment of the element (perfer sizes are a power of 2)
         * @param t_Offset      Offset of the element
         * @return void*        Obtain a chunk of memory of the size, alignment, and offset (asserts when we exceed preallocated size) 
         */
        void* Allocate(size_t t_Size, size_t t_Alignment, size_t t_Offset);

        /**
         * @brief Returns a block of memory to the stack in a LIFO manner 
         * 
         * @param t_Ptr 
         */
        void Free(void* t_Ptr);

    private:
        char* m_Start = nullptr;
        char* m_End = nullptr;
        char* m_Current = nullptr;
    };
}