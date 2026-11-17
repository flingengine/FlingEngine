#pragma once

#include "FlingTypes.h"
#include "FlingVulkan.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <deque>
#include <functional>
#include <memory>
#include <vector>

namespace Fling
{
    class CommandRunnable;
    class Thread;

    /**
     * A thread that owns the swap chain's two blocking calls: it presents the frames and it acquires the images.
     *
     * On some drivers (NVIDIA on Wayland) vkQueuePresentKHR blocks until the GPU has finished the frame.
     * Called on the render thread, that makes the CPU wait for the GPU every frame, and the two never work at the
     * same time. Here the render thread hands the present over and goes on with the next frame.
     *
     * A swap chain is externally synchronized: it must not be used by two threads at once. So this thread also
     * makes the acquire calls, one after each present, and the render thread takes the images that it got
     * (TakeAcquired). The render thread never calls the swap chain between Drain and the next Submit.
     *
     * Order of the work: the presents and the acquires run in the order they were given.
     */
    class PresentThread
    {
    public:
        /** An image that the thread acquired, and the semaphore that its acquire signals */
        struct Acquired
        {
            uint32 Index = 0;
            VkSemaphore Semaphore = VK_NULL_HANDLE;
            VkResult Result = VK_SUCCESS;
        };

        /**
         * @param t_Acquire     Acquires the next image of the swap chain: signals the semaphore, gives the image index.
         * @param t_Semaphores  The semaphores that the acquires signal, used in turn. At least FramesInFlight + 2: an
         *                      acquire reuses the semaphore of the one that was this many acquires earlier, whose
         *                      frame is finished by then. The vector must not change while the thread runs.
         */
        PresentThread(std::function<VkResult(VkSemaphore, uint32&)> t_Acquire,
                      const std::vector<VkSemaphore>* t_Semaphores);
        ~PresentThread();

        PresentThread(const PresentThread&) = delete;
        PresentThread& operator=(const PresentThread&) = delete;

        /** False when the OS cannot start the thread. Present nothing then. */
        bool IsRunning() const { return m_Thread != nullptr; }

        /** Acquire t_Count images ahead of the frames: call it after the swap chain is made, and after a Reset */
        void Prime(uint32 t_Count);

        /**
         * Run t_Present on the thread, then acquire the next image (for the frame that is FramesInFlight frames
         * later). The present's result is kept for TakeResult.
         */
        void SubmitPresent(std::function<VkResult()> t_Present);

        /** The next acquired image. Waits for the thread when it is not there yet. */
        Acquired TakeAcquired();

        /** Wait until every submitted present and acquire has returned. */
        void Drain();

        /**
         * Forget the images that were acquired and not taken, and start the semaphores again. Call it after Drain,
         * when the swap chain and the semaphores are made again.
         */
        void Reset();

        /**
         * The worst result of the presents since the last call (an out of date swap chain wins over a
         * suboptimal one), or VK_SUCCESS.
         */
        VkResult TakeResult();

    private:
        /** On the thread: acquire one image and queue it */
        void AcquireOne();

        const std::function<VkResult(VkSemaphore, uint32&)> m_Acquire;
        const std::vector<VkSemaphore>* const m_Semaphores;

        std::shared_ptr<CommandRunnable> m_Runnable;
        std::shared_ptr<Thread> m_Thread;
        std::atomic<uint32> m_Pending{0};
        std::atomic<int32> m_Result{VK_SUCCESS};

        /** The number of acquires made. The thread only. */
        uint64 m_AcquireCount = 0;

        Mutex m_Mutex;
        ConditionVariable m_Ready;
        std::deque<Acquired> m_Acquired FLING_GUARDED_BY(m_Mutex);
    };
} // namespace Fling
