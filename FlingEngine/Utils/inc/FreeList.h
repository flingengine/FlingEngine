#pragma once

#include <stdlib.h>     // size_t
#include <cstddef>		//std::ptrdiff_t
#include <assert.h>

#include "FlingExports.h"

namespace Fling
{
    /**
     * @brief   Helpful for allocating/freeing objects of a certain
     *          size which have to be created/destroeyed dynamically
     * 
     * @see     https://blog.molecular-matters.com/2012/09/17/memory-allocation-strategies-a-pool-allocator/
     */
    class FLING_API FreeList
    {
    public:

        /**
         * @brief Construct a new Free List object
         * 
         * @param t_Start       Start of the memory block to use for this free list
         * @param t_End         End of the memory block to use for this free list
         * @param t_ElmSize     Size of an "element" that this list will be used for
         * @param t_NumElms     Number of elements to store inside this free list
         * @param t_Alignment   Alignment of the element (default = 8)
         * @param t_Offset      Offset of the element (default = 0)
         */
        FreeList(void* t_Start, void* t_End, size_t t_ElmSize, size_t t_Alignment = 8, size_t t_Offset = 0);

        /**
         * @brief         Obtain a chunk of memory of the size and alignment that this list was created with
         * 
         * @return void*    nullptr if no memory available
         */
        inline void* Obtain() noexcept;

        /**
         * @brief   Return a block of memory to the free list. Memory can be returned in any order
         */
        inline void Return(void* t_Ptr);

    private:
    
        /** Alias in memory to the next available block. Nullptr if none are available */
        FreeList* m_Next = nullptr;
    };

	inline void* FreeList::Obtain() noexcept
	{
		if (m_Next == nullptr)
		{
			return nullptr;
		}

		FreeList* head = m_Next;
		m_Next = head->m_Next;
		return head;
	}

	inline void FreeList::Return(void* t_Ptr)
	{
		FreeList* head = static_cast<FreeList*>(t_Ptr);
		head->m_Next = m_Next;
		m_Next = head;
	}

}   // namespace Fling