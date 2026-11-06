#pragma once

#include "FlingTypes.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <chrono>
#include <utility>
#include <vector>

namespace Fling
{
    /**
     * Queue from many producer threads to one consumer thread.
     *
     * Any thread can Push. Only the consumer calls Drain and WaitForItems. Drain
     * takes all items under a short lock, then calls the function without the
     * lock, in push order. An item pushed during Drain goes to the next Drain.
     *
     * Prefer data items (a request struct) to lambdas when the items cross into
     * a subsystem. Data can be recorded, and it cannot hold a pointer into a world.
     */
    // #TODO: Replace the mutex and vector with a lock-free MPSC node queue, with the
    // same API. See Plans/lockless-threading.md
    template <typename T>
    class ThreadQueue
    {
    public:
        void Push(T t_Item)
        {
            {
                ScopedLock Lock(m_Mutex);
                m_Items.push_back(std::move(t_Item));
            }
            m_Condition.NotifyOne();
        }

        /**
         * Call t_Fn(T&) on each item that is in the queue now.
         *
         * @return The number of items
         */
        template <typename T_Fn>
        uint32 Drain(T_Fn&& t_Fn)
        {
            std::vector<T> Items;
            {
                ScopedLock Lock(m_Mutex);
                Items.swap(m_Items);
            }
            for (T& Item : Items)
            {
                t_Fn(Item);
            }
            return static_cast<uint32>(Items.size());
        }

        /**
         * Wait until an item is in the queue, Wake is called, or t_Timeout ends.
         *
         * @return True when an item is in the queue
         */
        bool WaitForItems(std::chrono::nanoseconds t_Timeout)
        {
            ScopedLock Lock(m_Mutex);
            m_Condition.WaitFor(
                Lock, t_Timeout, [this]() FLING_REQUIRES(m_Mutex) { return !m_Items.empty() || m_WakeRequested; });
            m_WakeRequested = false;
            return !m_Items.empty();
        }

        /** Make WaitForItems return now, or at its next call when no thread waits. */
        void Wake()
        {
            {
                ScopedLock Lock(m_Mutex);
                m_WakeRequested = true;
            }
            m_Condition.NotifyOne();
        }

        bool IsEmpty() const
        {
            ScopedLock Lock(m_Mutex);
            return m_Items.empty();
        }

    private:
        mutable Mutex m_Mutex;
        ConditionVariable m_Condition;
        std::vector<T> m_Items FLING_GUARDED_BY(m_Mutex);
        bool m_WakeRequested FLING_GUARDED_BY(m_Mutex) = false;
    };
} // namespace Fling
