#pragma once

#include "FlingTypes.h"

namespace Fling
{
   
	/**
	 * @brief	A simple circular buffer that will allow you get the next element in a buffer
	 *			It does not ensure that the item is not in use, but simply loops around.
	 */
    template<typename T, size_t t_Size>
    class CircularBuffer
    {
    public:
        
        CircularBuffer();

        ~CircularBuffer() = default;

		T* GetItem();

    private:
       
		const UINT32 m_BufferSize;
		
		T m_Buffer [t_Size];

		UINT32 m_AllocatedIndex = 0;
    };

	template<typename T, size_t t_Size>
	inline CircularBuffer<T, t_Size>::CircularBuffer()
		: m_BufferSize(t_Size)
		, m_AllocatedIndex(0)
		, m_Buffer{}
	{
	}

	template<typename T, size_t t_Size>
	inline T* CircularBuffer<T, t_Size>::GetItem()
	{
		const uint32_t index = m_AllocatedIndex++;

		return &(m_Buffer[index & (m_BufferSize - 1u)]);
	}
}   // namespace Fling