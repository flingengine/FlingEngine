#include "JoltJobSystem.h"
#include "Logger.h"

#include <algorithm>
#include <string>
#include <thread>

namespace Fling
{
    void JoltJobSystem::JobQueue::Push(Job** t_Jobs, JPH::uint t_Count, JPH::uint t_WorkersToWake)
    {
        {
            ScopedLock Lock(m_Mutex);
            m_Jobs.insert(m_Jobs.end(), t_Jobs, t_Jobs + t_Count);
        }
        m_WorkAvailable.Release(t_WorkersToWake);
    }

    JoltJobSystem::Job* JoltJobSystem::JobQueue::TryPop()
    {
        ScopedLock Lock(m_Mutex);
        if (m_Jobs.empty())
        {
            return nullptr;
        }

        Job* Front = m_Jobs.front();
        m_Jobs.pop_front();
        return Front;
    }

    uint32 JoltJobSystem::WorkerRunnable::Run()
    {
        while (!m_Quit.load(std::memory_order_acquire))
        {
            m_Queue.Wait();
            for (Job* Next = m_Queue.TryPop(); Next != nullptr; Next = m_Queue.TryPop())
            {
                RunJob(Next);
            }
        }
        return 0;
    }

    void JoltJobSystem::WorkerRunnable::Stop()
    {
        // Set the flag before any wake, then wake every worker. A worker that
        // wakes after this always sees the flag and returns.
        m_Quit.store(true, std::memory_order_release);
        m_Queue.Wake(m_WorkerCount);
    }

    void JoltJobSystem::RunJob(Job* t_Job)
    {
        // A job that the waiting thread already ran does nothing here. Jolt
        // makes sure that each job runs one time.
        t_Job->Execute();
        t_Job->Release();
    }

    JoltJobSystem::JoltJobSystem(JPH::uint t_MaxJobs, JPH::uint t_MaxBarriers, uint32 t_WorkerCount)
    {
        JobSystemWithBarrier::Init(t_MaxBarriers);
        m_JobStorage.Init(t_MaxJobs, t_MaxJobs);

        for (uint32 i = 0; i < t_WorkerCount; ++i)
        {
            ThreadDesc Desc;
            Desc.Name = "Physics Worker " + std::to_string(i + 1);

            std::shared_ptr<Thread> Worker =
                Thread::Create(std::make_shared<WorkerRunnable>(m_Queue, m_Quit, t_WorkerCount), Desc);
            if (!Worker)
            {
                // Fewer workers is still correct: the waiting thread runs the jobs
                F_LOG_WARN("Could not start {}. Physics uses {} workers", Desc.Name, m_Workers.size());
                break;
            }
            m_Workers.push_back(std::move(Worker));
        }
    }

    JoltJobSystem::~JoltJobSystem()
    {
        // The first Kill calls Stop, which makes every worker quit. Then each Kill joins.
        for (const std::shared_ptr<Thread>& Worker : m_Workers)
        {
            Worker->Kill();
        }
        m_Workers.clear();

        // Run the jobs that are left, so every job is released
        for (Job* Next = m_Queue.TryPop(); Next != nullptr; Next = m_Queue.TryPop())
        {
            RunJob(Next);
        }
    }

    JPH::JobHandle JoltJobSystem::CreateJob(const char* t_Name,
                                            JPH::ColorArg t_Color,
                                            const JobFunction& t_Function,
                                            JPH::uint32 t_NumDependencies)
    {
        JPH::uint32 Index = m_JobStorage.ConstructObject(t_Name, t_Color, this, t_Function, t_NumDependencies);
        while (Index == JobList::cInvalidObjectIndex)
        {
            // All job slots are in use. Wait for a worker to free one.
            F_LOG_WARN("Physics job system is full. Raise the job count");
            std::this_thread::yield();
            Index = m_JobStorage.ConstructObject(t_Name, t_Color, this, t_Function, t_NumDependencies);
        }

        Job* NewJob = &m_JobStorage.Get(Index);
        JobHandle Handle(NewJob);
        if (t_NumDependencies == 0)
        {
            QueueJob(NewJob);
        }
        return Handle;
    }

    void JoltJobSystem::QueueJob(Job* t_Job)
    {
        QueueJobs(&t_Job, 1);
    }

    void JoltJobSystem::QueueJobs(Job** t_Jobs, JPH::uint t_NumJobs)
    {
        // With no workers, the barrier wait runs the jobs on the calling thread
        if (m_Workers.empty() || t_NumJobs == 0)
        {
            return;
        }

        // The queue holds a reference until a worker runs the job
        for (JPH::uint i = 0; i < t_NumJobs; ++i)
        {
            t_Jobs[i]->AddRef();
        }
        m_Queue.Push(t_Jobs, t_NumJobs, std::min<JPH::uint>(t_NumJobs, static_cast<JPH::uint>(m_Workers.size())));
    }

    void JoltJobSystem::FreeJob(Job* t_Job)
    {
        m_JobStorage.DestructObject(t_Job);
    }
} // namespace Fling
