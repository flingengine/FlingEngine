#pragma once

#include "FlingTypes.h"

#include <chrono>
#include <condition_variable>
#include <mutex>
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
                std::lock_guard<std::mutex> Lock(m_Mutex);
                m_Items.push_back(std::move(t_Item));
            }
            m_Condition.notify_one();
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
                std::lock_guard<std::mutex> Lock(m_Mutex);
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
            std::unique_lock<std::mutex> Lock(m_Mutex);
            m_Condition.wait_for(Lock, t_Timeout, [this]() { return !m_Items.empty() || m_WakeRequested; });
            m_WakeRequested = false;
            return !m_Items.empty();
        }

        /** Make WaitForItems return now, or at its next call when no thread waits. */
        void Wake()
        {
            {
                std::lock_guard<std::mutex> Lock(m_Mutex);
                m_WakeRequested = true;
            }
            m_Condition.notify_one();
        }

        bool IsEmpty() const
        {
            std::lock_guard<std::mutex> Lock(m_Mutex);
            return m_Items.empty();
        }

    private:
        mutable std::mutex m_Mutex;
        std::condition_variable m_Condition;
        std::vector<T> m_Items;
        bool m_WakeRequested = false;
    };
} // namespace Fling
