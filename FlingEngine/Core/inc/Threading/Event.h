#pragma once

#include "FlingTypes.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <chrono>

namespace Fling
{
    /** How an Event treats the flag after a wait succeeds. */
    enum class EventReset : uint8
    {
        /** A wait that succeeds clears the flag. One Signal lets one waiter through. */
        Auto,
        /** The flag stays set until Reset. One Signal lets every waiter through. */
        Manual,
    };

    /**
     * A flag that threads can wait for.
     *
     * Signal sets the flag. Wait blocks until the flag is set. With Auto, the
     * wait that passes clears the flag. With Manual, the flag stays set until
     * Reset.
     */
    class Event
    {
    public:
        explicit Event(EventReset t_Reset, bool t_InitiallySet = false) : m_Reset(t_Reset), m_Set(t_InitiallySet) {}

        Event(const Event&) = delete;
        Event& operator=(const Event&) = delete;
        Event(Event&&) = delete;
        Event& operator=(Event&&) = delete;

        /** Set the flag and wake a waiter (Auto) or every waiter (Manual). */
        void Signal()
        {
            {
                ScopedLock Lock(m_Mutex);
                m_Set = true;
            }
            if (m_Reset == EventReset::Manual)
            {
                m_Condition.NotifyAll();
            }
            else
            {
                m_Condition.NotifyOne();
            }
        }

        /** Clear the flag. A later Wait blocks until the next Signal. */
        void Reset()
        {
            ScopedLock Lock(m_Mutex);
            m_Set = false;
        }

        /** Block until the flag is set. Auto then clears the flag. */
        void Wait()
        {
            ScopedLock Lock(m_Mutex);
            m_Condition.Wait(Lock, [this]() FLING_REQUIRES(m_Mutex) { return m_Set; });
            if (m_Reset == EventReset::Auto)
            {
                m_Set = false;
            }
        }

        /**
         * Wait until the flag is set or t_Timeout ends.
         *
         * @return True when the flag was set. Auto then clears the flag.
         */
        bool WaitFor(std::chrono::nanoseconds t_Timeout)
        {
            ScopedLock Lock(m_Mutex);
            const bool Signaled =
                m_Condition.WaitFor(Lock, t_Timeout, [this]() FLING_REQUIRES(m_Mutex) { return m_Set; });
            if (Signaled && m_Reset == EventReset::Auto)
            {
                m_Set = false;
            }
            return Signaled;
        }

        bool IsSet() const
        {
            ScopedLock Lock(m_Mutex);
            return m_Set;
        }

    private:
        mutable Mutex m_Mutex;
        ConditionVariable m_Condition;
        const EventReset m_Reset;
        bool m_Set FLING_GUARDED_BY(m_Mutex);
    };
} // namespace Fling
