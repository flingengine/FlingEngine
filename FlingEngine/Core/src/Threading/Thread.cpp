#include "Strings/String.h"
#include "Threading/CommandRunnable.h"
#include "Threading/ConditionVariable.h"
#include "Threading/Mutex.h"
#include "Threading/NativeThread.h"
#include "Threading/Thread.h"
#include "Threading/ThreadManager.h"
#include "Threading/ThreadSafety.h"

#include <cassert>
#include <utility>

namespace Fling
{
    namespace
    {
        thread_local ThreadId t_CurrentId = InvalidThreadId;
        thread_local String t_CurrentName;

        /** The Runnable of the lambda form of Thread::Create. */
        class LambdaRunnable final : public Runnable
        {
        public:
            explicit LambdaRunnable(std::function<void()> t_Function) : m_Function(std::move(t_Function)) {}

            uint32 Run() override
            {
                m_Function();
                return 0;
            }

        private:
            std::function<void()> m_Function;
        };

        /** The Runnable of the fixed-rate lambda form of Thread::Create. */
        class TickingLambdaRunnable final : public CommandRunnable
        {
        public:
            TickingLambdaRunnable(TickRate t_Rate, std::function<void()> t_Tick)
                : CommandRunnable(t_Rate), m_Tick(std::move(t_Tick))
            {
            }

            void Tick() override { m_Tick(); }

        private:
            std::function<void()> m_Tick;
        };
    } // namespace

    Thread::Thread(std::shared_ptr<Runnable> t_Runnable, ThreadDesc t_Desc, ThreadId t_Id)
        : m_Runnable(std::move(t_Runnable)), m_Desc(std::move(t_Desc)), m_Id(t_Id)
    {
    }

    std::shared_ptr<Thread> Thread::Create(std::shared_ptr<Runnable> t_Runnable, ThreadDesc t_Desc)
    {
        if (!t_Runnable)
        {
            return nullptr;
        }

        // The constructor is private, so make_shared cannot call it
        std::shared_ptr<Thread> NewThread(
            new Thread(std::move(t_Runnable), std::move(t_Desc), ThreadManager::AllocateId()));

        // Hold the start lock while m_Native is set, so Entry cannot read a half-set Thread.
        // m_Native is guarded by the join mutex. No other thread can take that lock yet:
        // the shared_ptr is not published, and Entry blocks on the start lock.
        ScopedLock Lock(NewThread->m_StartMutex);
        bool HasNative = false;
        {
            ScopedLock JoinLock(NewThread->m_JoinMutex);
            NewThread->m_Native = NativeThread::Start(&Thread::Entry, NewThread.get(), NewThread->m_Desc.StackSize);
            HasNative = static_cast<bool>(NewThread->m_Native);
        }
        if (!HasNative)
        {
            return nullptr;
        }

        NewThread->m_StartCondition.Wait(Lock,
                                         [&NewThread]() FLING_REQUIRES(NewThread->m_StartMutex)
                                         { return NewThread->m_StartState != StartState::Pending; });
        const bool Started = NewThread->m_StartState == StartState::Started;
        Lock.Unlock();

        if (!Started)
        {
            NewThread->Join();
            return nullptr;
        }
        return NewThread;
    }

    std::shared_ptr<Thread> Thread::Create(ThreadDesc t_Desc, std::function<void()> t_Function)
    {
        return Create(std::make_shared<LambdaRunnable>(std::move(t_Function)), std::move(t_Desc));
    }

    std::shared_ptr<Thread> Thread::Create(ThreadDesc t_Desc, TickRate t_Rate, std::function<void()> t_Tick)
    {
        return Create(std::make_shared<TickingLambdaRunnable>(t_Rate, std::move(t_Tick)), std::move(t_Desc));
    }

    Thread::~Thread()
    {
        assert(CurrentId() != m_Id && "A Thread cannot be destroyed on its own thread");
        if (CurrentId() != m_Id)
        {
            Kill(true);
        }
        // Else ~NativeThread detaches it
    }

    void Thread::Entry(void* t_Thread)
    {
        Thread& Self = *static_cast<Thread*>(t_Thread);
        ScopedLock Lock(Self.m_StartMutex);

        t_CurrentId = Self.m_Id;
        t_CurrentName = Self.m_Desc.Name;
        Self.m_NativeId = NativeThread::CurrentNativeId();
        Self.m_Running.store(true, std::memory_order_release);

        NativeThread::SetCurrentName(Self.m_Desc.Name);
        if (Self.m_Desc.Priority != ThreadPriority::Normal)
        {
            NativeThread::SetCurrentPriority(Self.m_Desc.Priority);
        }
        NativeThread::SetCurrentAffinity(Self.m_Desc.AffinityMask);
        ThreadManager::Register(Self.m_Id, Self.m_Desc.Name, Self.m_NativeId);

        const bool InitOk = Self.m_Runnable->Init();
        Self.m_StartState = InitOk ? StartState::Started : StartState::InitFailed;
        Lock.Unlock();
        Self.m_StartCondition.NotifyAll();

        if (InitOk)
        {
            Self.m_Runnable->Run();
            Self.m_Runnable->Exit();
        }

        ThreadManager::Unregister(Self.m_Id);
        Self.m_Running.store(false, std::memory_order_release);
    }

    void Thread::Kill(bool t_Wait)
    {
        m_Runnable->Stop();
        if (t_Wait)
        {
            Join();
        }
    }

    void Thread::Join()
    {
        assert(CurrentId() != m_Id && "A Thread cannot join itself");
        ScopedLock Lock(m_JoinMutex);
        if (m_Native)
        {
            m_Native->Join();
            m_Native.reset();
        }
    }

    bool Thread::SetPriority(ThreadPriority t_Priority)
    {
        ScopedLock Lock(m_JoinMutex);
        if (!m_Native || !IsRunning())
        {
            return false;
        }
        if (!m_Native->SetPriority(t_Priority, m_NativeId))
        {
            return false;
        }
        m_Desc.Priority = t_Priority;
        return true;
    }

    void Thread::InitGameThread(StringView t_Name)
    {
        t_CurrentId = GameThreadId;
        t_CurrentName = String(t_Name);
        NativeThread::SetCurrentName(t_Name);
        ThreadManager::Register(GameThreadId, t_Name, NativeThread::CurrentNativeId());
    }

    ThreadId Thread::CurrentId()
    {
        return t_CurrentId;
    }

    const String& Thread::CurrentName()
    {
        return t_CurrentName;
    }
} // namespace Fling
