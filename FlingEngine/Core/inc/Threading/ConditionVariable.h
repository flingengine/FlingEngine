#pragma once

#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <cassert>
#include <chrono>
#include <condition_variable>
#include <mutex>

namespace Fling
{
    /**
     * Waits until a notify, or until a predicate is true.
     *
     * Pass the ScopedLock that holds the mutex. The wait can unlock that mutex
     * while the thread sleeps. The ScopedLock is locked again when the wait
     * returns.
     */
    class ConditionVariable
    {
    public:
        ConditionVariable() = default;

        ConditionVariable(const ConditionVariable&) = delete;
        ConditionVariable& operator=(const ConditionVariable&) = delete;
        ConditionVariable(ConditionVariable&&) = delete;
        ConditionVariable& operator=(ConditionVariable&&) = delete;

        void NotifyOne() { m_Condition.notify_one(); }

        void NotifyAll() { m_Condition.notify_all(); }

        void Wait(ScopedLock& t_Lock) FLING_NO_THREAD_SAFETY_ANALYSIS
        {
            AdoptedLock Adopted(t_Lock);
            m_Condition.wait(Adopted.Get());
        }

        template <typename T_Pred>
        void Wait(ScopedLock& t_Lock, T_Pred t_Pred) FLING_NO_THREAD_SAFETY_ANALYSIS
        {
            AdoptedLock Adopted(t_Lock);
            m_Condition.wait(Adopted.Get(), std::move(t_Pred));
        }

        /**
         * Wait until t_Pred is true or t_Timeout ends.
         *
         * @return True when t_Pred is true
         */
        template <typename T_Pred>
        bool
        WaitFor(ScopedLock& t_Lock, std::chrono::nanoseconds t_Timeout, T_Pred t_Pred) FLING_NO_THREAD_SAFETY_ANALYSIS
        {
            AdoptedLock Adopted(t_Lock);
            return m_Condition.wait_for(Adopted.Get(), t_Timeout, std::move(t_Pred));
        }

        /**
         * Wait until t_Pred is true or t_Deadline is reached.
         *
         * @return True when t_Pred is true
         */
        template <typename T_Clock, typename T_Duration, typename T_Pred>
        bool WaitUntil(ScopedLock& t_Lock,
                       const std::chrono::time_point<T_Clock, T_Duration>& t_Deadline,
                       T_Pred t_Pred) FLING_NO_THREAD_SAFETY_ANALYSIS
        {
            AdoptedLock Adopted(t_Lock);
            return m_Condition.wait_until(Adopted.Get(), t_Deadline, std::move(t_Pred));
        }

    private:
        /** The ScopedLock must already hold its mutex. */
        static std::mutex& LockedMutex(ScopedLock& t_Lock)
        {
            assert(t_Lock.m_Locked && "The ScopedLock must be locked");
            return t_Lock.m_Mutex.m_Mutex;
        }

        /**
         * Holds the std::mutex that a ScopedLock already locked, for one wait.
         * The destructor gives the mutex back, so the ScopedLock still owns it.
         */
        class AdoptedLock
        {
        public:
            explicit AdoptedLock(ScopedLock& t_Lock) : m_Lock(LockedMutex(t_Lock), std::adopt_lock) {}

            ~AdoptedLock() { m_Lock.release(); }

            AdoptedLock(const AdoptedLock&) = delete;
            AdoptedLock& operator=(const AdoptedLock&) = delete;
            AdoptedLock(AdoptedLock&&) = delete;
            AdoptedLock& operator=(AdoptedLock&&) = delete;

            std::unique_lock<std::mutex>& Get() { return m_Lock; }

        private:
            std::unique_lock<std::mutex> m_Lock;
        };

        std::condition_variable m_Condition;
    };
} // namespace Fling
