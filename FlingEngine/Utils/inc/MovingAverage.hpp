#pragma once

#include "FlingTypes.h"

namespace Fling
{
    /**
     * @brief A moving average can be used to calculate things like FPS
     */
    template<typename T, size_t t_Size>
    class MovingAverage
    {
    public:
        
        MovingAverage();
        ~MovingAverage() = default;

        /** Push a value onto the buffer to be calculated */
        void Push(T t_Element);

        /** Get the average (Sum / num elements) */
        T GetAverage() const;
    
    private:
        /** Index for keeping track of what part of the buffer we should be inserting into */
        size_t m_CurrentIndex = {};

        /** Current count of things pushed to this average */
        size_t m_Count = {};
        
        /** Max sample size in this moving average */
        size_t m_MaxSize = {};

        /** Buffer of samples used to calculate the moving average */
        T m_Buffer [t_Size] = {};
    };

    template<typename T, size_t t_Size>
    inline MovingAverage<T, t_Size>::MovingAverage()
        : m_MaxSize(t_Size)
    {
    }

    template<typename T, size_t t_Size>
    inline void MovingAverage<T, t_Size>::Push(T t_Element)
    {
        ++m_Count;
        const uint32 index = m_CurrentIndex++;
        m_Buffer[index & (m_MaxSize - 1u)] = t_Element;
    }

    template<typename T, size_t t_Size>
    inline T MovingAverage<T, t_Size>::GetAverage() const
    {
        T Average = {};

        size_t Range = (m_Count < m_MaxSize ? m_Count : m_MaxSize);
        for (size_t i = 0; i < Range; ++i)
        {
            Average += m_Buffer[i];
        }

        return (Average / static_cast<T>(Range));
    }
}   // namespace Fling