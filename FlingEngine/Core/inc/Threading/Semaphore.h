#pragma once

#include "FlingTypes.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <chrono>

namespace Fling
{
    /**
     * A counter of permits.
     *
     * Acquire takes one permit and waits when the count is 0. Release adds
     * permits. The constructor count is the number of permits at the start.
     *
     * It is a count, a Mutex, and a ConditionVariable, not std::counting_semaphore.
     * The libstdc++ std::counting_semaphore of GCC 13 can miss a wake under
     * contention, so a waiter sleeps forever while permits are available (a hang
     * in the "Semaphore keeps the number of holders at its count" test on CI).
     */
    class Semaphore
    {
    public:
        explicit Semaphore(uint32 t_Initial = 0) : m_Count(t_Initial) {}

        Semaphore(const Semaphore&) = delete;
        Semaphore& operator=(const Semaphore&) = delete;
        Semaphore(Semaphore&&) = delete;
        Semaphore& operator=(Semaphore&&) = delete;

        /**
         * Add t_Count permits, and wake up to t_Count waiters.
         *
         * The notify is sent while the mutex is held. A waiter can then not
         * return and destroy the Semaphore before this call is done with it.
         */
        void Release(uint32 t_Count = 1)
        {
            if (t_Count == 0)
            {
                return;
            }

            ScopedLock Lock(m_Mutex);
            m_Count += t_Count;
            if (t_Count >= m_Waiters)
            {
                m_Available.NotifyAll();
            }
            else
            {
                for (uint32 Index = 0; Index < t_Count; ++Index)
                {
                    m_Available.NotifyOne();
                }
            }
        }

        /** Take one permit. Wait until a permit is available. */
        void Acquire()
        {
            ScopedLock Lock(m_Mutex);
            ++m_Waiters;
            m_Available.Wait(Lock, [this]() FLING_REQUIRES(m_Mutex) { return m_Count > 0; });
            --m_Waiters;
            --m_Count;
        }

        /** Take one permit when one is available. Return false when the count is 0. */
        bool TryAcquire()
        {
            ScopedLock Lock(m_Mutex);
            if (m_Count == 0)
            {
                return false;
            }
            --m_Count;
            return true;
        }

        /**
         * Take one permit, or return false when t_Timeout ends first. A timeout
         * too long for the clock waits until a permit is available.
         */
        template <typename T_Rep, typename T_Period>
        bool TryAcquireFor(const std::chrono::duration<T_Rep, T_Period>& t_Timeout)
        {
            ScopedLock Lock(m_Mutex);
            ++m_Waiters;
            const bool bAvailable =
                m_Available.WaitFor(Lock, t_Timeout, [this]() FLING_REQUIRES(m_Mutex) { return m_Count > 0; });
            --m_Waiters;
            if (!bAvailable)
            {
                return false;
            }
            --m_Count;
            return true;
        }

    private:
        Mutex m_Mutex;
        ConditionVariable m_Available;

        /** Permits that no one holds. */
        uint32 m_Count FLING_GUARDED_BY(m_Mutex);

        /** Threads in Acquire or TryAcquireFor that wait for a permit. */
        uint32 m_Waiters FLING_GUARDED_BY(m_Mutex) = 0;
    };
} // namespace Fling
