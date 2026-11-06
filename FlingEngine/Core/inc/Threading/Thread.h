#pragma once

#include "CoreAPI.h"
#include "Threading/Runnable.h"
#include "Threading/ThreadTypes.h"

#include <atomic>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>

namespace Fling
{
    class NativeThread;

    /**
     * One OS thread with an engine name and an engine ID.
     *
     * Create starts the thread and waits until Runnable::Init returns, so the ID
     * and the name are ready when Create returns. Kill calls Runnable::Stop and
     * joins. The destructor joins a thread that is still running.
     *
     * Do not use a Thread from an ECS system. A subsystem (audio, render) owns its
     * thread. Gameplay sends it work through a queue (see Skills/threading.md).
     */
    class Thread
    {
    public:
        /**
         * Start a thread that runs t_Runnable.
         *
         * @return Null when the OS cannot start the thread, or when Runnable::Init
         *         returns false. Run is not called in those cases.
         */
        CORE_API static std::shared_ptr<Thread> Create(std::shared_ptr<Runnable> t_Runnable, ThreadDesc t_Desc);

        /**
         * Start a thread that calls t_Function one time. Kill cannot stop
         * t_Function. Give it a flag of its own when it loops.
         */
        CORE_API static std::shared_ptr<Thread> Create(ThreadDesc t_Desc, std::function<void()> t_Function);

        /**
         * Start a thread that calls t_Tick at t_Rate until Kill, for example
         * TickRate::Hz(120). This is a CommandRunnable with a fixed rate. Use a
         * CommandRunnable directly to also send commands to the thread.
         */
        CORE_API static std::shared_ptr<Thread>
        Create(ThreadDesc t_Desc, TickRate t_Rate, std::function<void()> t_Tick);

        /** Join the thread when it is still running. Do not destroy a Thread on its own thread. */
        CORE_API ~Thread();

        Thread(const Thread&) = delete;
        Thread& operator=(const Thread&) = delete;
        Thread(Thread&&) = delete;
        Thread& operator=(Thread&&) = delete;

        ThreadId GetId() const { return m_Id; }

        const std::string& GetName() const { return m_Desc.Name; }

        /** The OS ID (gettid on Linux, GetCurrentThreadId on Windows), for debuggers and logs. */
        uint64 GetNativeId() const { return m_NativeId; }

        /** True from before Runnable::Init until after Runnable::Exit. */
        bool IsRunning() const { return m_Running.load(std::memory_order_acquire); }

        /** Call Runnable::Stop. When t_Wait is true, also Join. */
        CORE_API void Kill(bool t_Wait = true);

        /** Wait until the thread ends. A second call does nothing. */
        CORE_API void Join();

        /** Change the priority. Return false when the OS refuses (for example, a raise on Linux). */
        CORE_API bool SetPriority(ThreadPriority t_Priority);

        /**
         * Give the calling thread GameThreadId and the name t_Name. Call this once,
         * at the start of the process, on the thread that runs the game loop.
         */
        CORE_API static void InitGameThread(std::string_view t_Name = "Game Thread");

        /** The engine ID of the calling thread, or InvalidThreadId when the engine did not start it. */
        CORE_API static ThreadId CurrentId();

        /** The engine name of the calling thread, or an empty string. */
        CORE_API static const std::string& CurrentName();

        static bool IsGameThread() { return CurrentId() == GameThreadId; }

    private:
        enum class StartState : uint8
        {
            Pending,
            Started,
            InitFailed,
        };

        Thread(std::shared_ptr<Runnable> t_Runnable, ThreadDesc t_Desc, ThreadId t_Id);

        /** The first function on the new thread. */
        static void Entry(void* t_Thread);

        std::shared_ptr<Runnable> m_Runnable;
        ThreadDesc m_Desc;
        const ThreadId m_Id;
        uint64 m_NativeId = 0;
        std::atomic<bool> m_Running{false};

        std::unique_ptr<NativeThread> m_Native;
        std::mutex m_JoinMutex;

        // Create waits on this until Entry has called Runnable::Init
        std::mutex m_StartMutex;
        std::condition_variable m_StartCondition;
        StartState m_StartState = StartState::Pending;
    };
} // namespace Fling
