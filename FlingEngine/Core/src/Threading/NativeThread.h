#pragma once

#include "Platform.h"
#include "Strings/String.h"
#include "Threading/ThreadTypes.h"

#include <memory>

#if !FLING_WINDOWS
#include <pthread.h>
#endif

namespace Fling
{
    /**
     * The OS part of a Thread. NativeThreadPosix.cpp and NativeThreadWindows.cpp
     * implement it. Only Thread.cpp uses it.
     */
    class NativeThread
    {
    public:
        using EntryFn = void (*)(void*);

        /** Start a thread that calls t_Entry(t_Arg). Return null when the OS cannot start it. */
        static std::unique_ptr<NativeThread> Start(EntryFn t_Entry, void* t_Arg, uint32 t_StackSize);

        /** Detach the thread when it was not joined. */
        ~NativeThread();

        NativeThread(const NativeThread&) = delete;
        NativeThread& operator=(const NativeThread&) = delete;

        /** Wait until the thread ends. Call this one time. */
        void Join();

        /** Change the priority of this thread. t_NativeId is the ID that the thread got from CurrentNativeId. */
        bool SetPriority(ThreadPriority t_Priority, uint64 t_NativeId);

        /** Give the calling thread an OS name for debuggers and profilers. */
        static void SetCurrentName(StringView t_Name);

        static bool SetCurrentPriority(ThreadPriority t_Priority);

        /** Limit the calling thread to the CPUs in t_Mask. 0 does nothing. */
        static bool SetCurrentAffinity(uint64 t_Mask);

        static uint64 CurrentNativeId();

    private:
        NativeThread() = default;

#if FLING_WINDOWS
        void* m_Handle = nullptr;
#else
        pthread_t m_Thread{};
#endif
        bool m_Joined = false;
    };
} // namespace Fling
