#pragma once

#include "FlingTypes.h"

#include <chrono>
#include <cstddef>
#include <semaphore>

namespace Fling
{
    /**
     * A counter of permits.
     *
     * Acquire takes one permit and waits when the count is 0. Release adds
     * permits. The constructor count is the number of permits at the start.
     */
    class Semaphore
    {
    public:
        explicit Semaphore(uint32 t_Initial = 0) : m_Semaphore(static_cast<std::ptrdiff_t>(t_Initial)) {}

        Semaphore(const Semaphore&) = delete;
        Semaphore& operator=(const Semaphore&) = delete;
        Semaphore(Semaphore&&) = delete;
        Semaphore& operator=(Semaphore&&) = delete;

        /** Add t_Count permits. */
        void Release(uint32 t_Count = 1) { m_Semaphore.release(static_cast<std::ptrdiff_t>(t_Count)); }

        /** Take one permit. Wait until a permit is available. */
        void Acquire() { m_Semaphore.acquire(); }

        /** Take one permit when one is available. Return false when the count is 0. */
        bool TryAcquire() { return m_Semaphore.try_acquire(); }

        /** Take one permit, or return false when t_Timeout ends first. */
        bool TryAcquireFor(std::chrono::nanoseconds t_Timeout) { return m_Semaphore.try_acquire_for(t_Timeout); }

    private:
        std::counting_semaphore<> m_Semaphore;
    };
} // namespace Fling
