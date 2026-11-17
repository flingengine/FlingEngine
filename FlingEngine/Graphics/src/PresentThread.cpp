#include "Logger.h"
#include "PresentThread.h"
#include "Profiler.h"
#include "Threading/CommandRunnable.h"
#include "Threading/Thread.h"

#include <chrono>
#include <thread>

namespace Fling
{
    PresentThread::PresentThread(std::function<VkResult(VkSemaphore, uint32&)> t_Acquire,
                                 const std::vector<VkSemaphore>* t_Semaphores)
        : m_Acquire(std::move(t_Acquire)), m_Semaphores(t_Semaphores)
    {
        m_Runnable = std::make_shared<CommandRunnable>();
        m_Thread = Thread::Create(m_Runnable, {.Name = "Present", .Priority = ThreadPriority::Normal});
        if (m_Thread == nullptr)
        {
            F_LOG_WARN("Cannot start the present thread; the render thread presents");
            m_Runnable.reset();
        }
    }

    PresentThread::~PresentThread()
    {
        if (m_Thread != nullptr)
        {
            Drain();
            m_Thread->Kill();
            m_Thread.reset();
        }
        m_Runnable.reset();
    }

    void PresentThread::AcquireOne()
    {
        FLING_PROFILE_SCOPE("Present.Acquire");
        Acquired Image;
        Image.Semaphore = (*m_Semaphores)[m_AcquireCount % m_Semaphores->size()];
        ++m_AcquireCount;
        Image.Result = m_Acquire(Image.Semaphore, Image.Index);
        // With every image busy the driver can say NOT_READY though the wait has no time limit: ask again
        while (Image.Result == VK_NOT_READY || Image.Result == VK_TIMEOUT)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(100));
            Image.Result = m_Acquire(Image.Semaphore, Image.Index);
        }
        {
            ScopedLock Lock(m_Mutex);
            m_Acquired.push_back(Image);
        }
        m_Ready.NotifyAll();
    }

    void PresentThread::Prime(const uint32 t_Count)
    {
        for (uint32 i = 0; i < t_Count; ++i)
        {
            m_Pending.fetch_add(1, std::memory_order_acq_rel);
            m_Runnable->Enqueue(
                [this]()
                {
                    AcquireOne();
                    m_Pending.fetch_sub(1, std::memory_order_acq_rel);
                });
        }
    }

    void PresentThread::SubmitPresent(std::function<VkResult()> t_Present)
    {
        m_Pending.fetch_add(1, std::memory_order_acq_rel);
        m_Runnable->Enqueue(
            [this, Present = std::move(t_Present)]()
            {
                VkResult Result = VK_SUCCESS;
                {
                    FLING_PROFILE_SCOPE("Present.Thread");
                    Result = Present();
                }
                // An out of date swap chain matters more than a suboptimal one, and any error more than both
                int32 Seen = m_Result.load(std::memory_order_acquire);
                auto Rank = [](const int32 t_Value)
                {
                    return t_Value == VK_SUCCESS ? 0 : (t_Value == VK_SUBOPTIMAL_KHR ? 1 : 2);
                };
                while (Rank(static_cast<int32>(Result)) > Rank(Seen) &&
                       !m_Result.compare_exchange_weak(Seen, static_cast<int32>(Result), std::memory_order_acq_rel))
                {
                }
                // The image of the frame that comes after the ones in flight
                if (Result == VK_SUCCESS || Result == VK_SUBOPTIMAL_KHR)
                {
                    AcquireOne();
                }
                else
                {
                    // The render thread waits for an image: tell it that the swap chain is no good
                    Acquired Failed;
                    Failed.Result = Result;
                    {
                        ScopedLock Lock(m_Mutex);
                        m_Acquired.push_back(Failed);
                    }
                    m_Ready.NotifyAll();
                }
                m_Pending.fetch_sub(1, std::memory_order_acq_rel);
            });
    }

    PresentThread::Acquired PresentThread::TakeAcquired()
    {
        ScopedLock Lock(m_Mutex);
        m_Ready.Wait(Lock, [this]() FLING_REQUIRES(m_Mutex) { return !m_Acquired.empty(); });
        const Acquired Image = m_Acquired.front();
        m_Acquired.pop_front();
        return Image;
    }

    void PresentThread::Drain()
    {
        while (m_Pending.load(std::memory_order_acquire) != 0)
        {
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
    }

    void PresentThread::Reset()
    {
        ScopedLock Lock(m_Mutex);
        m_Acquired.clear();
        m_AcquireCount = 0;
    }

    VkResult PresentThread::TakeResult()
    {
        return static_cast<VkResult>(m_Result.exchange(VK_SUCCESS, std::memory_order_acq_rel));
    }
} // namespace Fling
