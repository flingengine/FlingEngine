#pragma once

#include "FlingTypes.h"

namespace Fling
{
   
	/**
	 * @brief 	A simple circular buffer that will allow you get the next element in a buffer
	 *			It does not ensure that the item is not in use, but simply loops around.
	 * 
	 * @tparam T 		the type inside this circular buffer. Stack allocated
	 * @tparam t_Size 	The max size of this buffer, must be a power of 2!
	 */
    template<typename T, size_t t_NumElms>
    class CircularBuffer
    {
    public:
        
        CircularBuffer();

        ~CircularBuffer() = default;

		T* GetItem();

    private:
       
		const UINT32 m_BufferSize;
		
		T m_Buffer [t_NumElms];

		UINT32 m_AllocatedIndex = 0;
    };

	template<typename T, size_t t_NumElms>
	inline CircularBuffer<T, t_NumElms>::CircularBuffer()
		: m_BufferSize(t_NumElms)
		, m_AllocatedIndex(0)
		, m_Buffer{}
	{
		static_assert((t_NumElms != 0 && (t_NumElms & (t_NumElms-1)) == 0), "CircularBuffer::t_NumElms must be a power of 2!");
	}

	template<typename T, size_t t_NumElms>
	inline T* CircularBuffer<T, t_NumElms>::GetItem()
	{
		const uint32_t index = m_AllocatedIndex++;
		// This bit shifting is the same as the % operator
		// @see https://blog.molecular-matters.com/2015/09/08/job-system-2-0-lock-free-work-stealing-part-2-a-specialized-allocator/
		// for more about that
		return &(m_Buffer[(index - 1u) & (m_BufferSize - 1u)]);
	}
}   // namespace Fling