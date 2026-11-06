#pragma once

// clang-format off
#include <Jolt/Jolt.h>
// clang-format on

#include "Threading/Mutex.h"
#include "Threading/Runnable.h"
#include "Threading/Semaphore.h"
#include "Threading/Thread.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <deque>
#include <Jolt/Core/FixedSizeFreeList.h>
#include <Jolt/Core/JobSystemWithBarrier.h>
#include <memory>
#include <vector>

namespace Fling
{
    /**
     * Jolt's job system on Fling threads. Each worker is a Fling::Thread named
     * "Physics Worker N", so it has an engine thread ID and is in ThreadManager.
     *
     * Fork and join: the thread that calls PhysicsSystem::Update (the game
     * thread) pushes jobs, then runs jobs itself while it waits for the barrier.
     * With 0 workers, it runs every job, so the step is single-threaded. Workers
     * run only Jolt jobs. They never touch the registry (Skills/threading.md).
     *
     * Jolt gives the same result with any number of workers, so the worker
     * count is not part of the simulation state.
     */
    class JoltJobSystem final : public JPH::JobSystemWithBarrier
    {
    public:
        JoltJobSystem(JPH::uint t_MaxJobs, JPH::uint t_MaxBarriers, uint32 t_WorkerCount);
        ~JoltJobSystem() override;

        JoltJobSystem(const JoltJobSystem&) = delete;
        JoltJobSystem& operator=(const JoltJobSystem&) = delete;

        int GetMaxConcurrency() const override { return static_cast<int>(m_Workers.size()) + 1; }

        JobHandle CreateJob(const char* t_Name,
                            JPH::ColorArg t_Color,
                            const JobFunction& t_Function,
                            JPH::uint32 t_NumDependencies = 0) override;

        uint32 GetWorkerCount() const { return static_cast<uint32>(m_Workers.size()); }

    protected:
        void QueueJob(Job* t_Job) override;
        void QueueJobs(Job** t_Jobs, JPH::uint t_NumJobs) override;
        void FreeJob(Job* t_Job) override;

    private:
        /** The jobs that wait for a worker. Many threads push, and many workers pop. */
        class JobQueue
        {
        public:
            void Push(Job** t_Jobs, JPH::uint t_Count, JPH::uint t_WorkersToWake);

            /** The oldest job, or nullptr if the queue is empty. */
            Job* TryPop();

            /** Wait until a job is pushed or Wake is called. */
            void Wait() { m_WorkAvailable.Acquire(); }

            /** Wake t_Count waiting workers, with or without work. */
            void Wake(JPH::uint t_Count) { m_WorkAvailable.Release(t_Count); }

        private:
            Mutex m_Mutex;
            std::deque<Job*> m_Jobs FLING_GUARDED_BY(m_Mutex);
            Semaphore m_WorkAvailable;
        };

        /**
         * One worker: runs queued jobs until the pool quits. The quit flag is
         * shared by every worker. One flag per worker can deadlock: the workers
         * share one semaphore, so the wake for one worker can be taken by another
         * worker that does not quit, and the first one then never wakes.
         */
        class WorkerRunnable final : public Runnable
        {
        public:
            WorkerRunnable(JobQueue& t_Queue, std::atomic<bool>& t_Quit, JPH::uint t_WorkerCount)
                : m_Queue(t_Queue), m_Quit(t_Quit), m_WorkerCount(t_WorkerCount)
            {
            }

            uint32 Run() override;

            /** Make every worker of the pool quit. Safe to call more than one time. */
            void Stop() override;

        private:
            JobQueue& m_Queue;
            std::atomic<bool>& m_Quit;
            JPH::uint m_WorkerCount = 0;
        };

        /** Run a queued job and drop the queue's reference to it. */
        static void RunJob(Job* t_Job);

        using JobList = JPH::FixedSizeFreeList<Job>;
        JobList m_JobStorage;

        JobQueue m_Queue;
        std::atomic<bool> m_Quit{false};
        std::vector<std::shared_ptr<Thread>> m_Workers;
    };
} // namespace Fling
