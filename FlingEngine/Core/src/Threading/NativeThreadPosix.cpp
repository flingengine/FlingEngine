#include "Platform.h"

#if !FLING_WINDOWS

#include "Strings/String.h"
#include "Threading/NativeThread.h"

#include <memory>
#include <sched.h>
#include <sys/resource.h>
#include <sys/syscall.h>
#include <unistd.h>

namespace Fling
{
    namespace
    {
        struct StartInfo
        {
            NativeThread::EntryFn Entry;
            void* Arg;
        };

        void* Trampoline(void* t_Info)
        {
            const std::unique_ptr<StartInfo> Info(static_cast<StartInfo*>(t_Info));
            Info->Entry(Info->Arg);
            return nullptr;
        }

        // Linux threads have their own nice value. A normal user can only raise
        // it (lower the priority) without CAP_SYS_NICE.
        int NiceValue(ThreadPriority t_Priority)
        {
            switch (t_Priority)
            {
            case ThreadPriority::Lowest:
                return 10;
            case ThreadPriority::BelowNormal:
                return 5;
            case ThreadPriority::Normal:
                return 0;
            case ThreadPriority::AboveNormal:
                return -5;
            case ThreadPriority::Highest:
                return -10;
            case ThreadPriority::TimeCritical:
                return -15;
            }
            return 0;
        }

        bool SetNice(ThreadPriority t_Priority, uint64 t_NativeId)
        {
            return setpriority(PRIO_PROCESS, static_cast<id_t>(t_NativeId), NiceValue(t_Priority)) == 0;
        }
    } // namespace

    std::unique_ptr<NativeThread> NativeThread::Start(EntryFn t_Entry, void* t_Arg, uint32 t_StackSize)
    {
        pthread_attr_t Attr;
        if (pthread_attr_init(&Attr) != 0)
        {
            return nullptr;
        }
        if (t_StackSize > 0)
        {
            pthread_attr_setstacksize(&Attr, t_StackSize);
        }

        std::unique_ptr<NativeThread> Native(new NativeThread());
        auto Info = std::make_unique<StartInfo>(StartInfo{t_Entry, t_Arg});
        const int Result = pthread_create(&Native->m_Thread, &Attr, &Trampoline, Info.get());
        pthread_attr_destroy(&Attr);

        if (Result != 0)
        {
            return nullptr;
        }
        // The new thread owns Info now
        Info.release();
        return Native;
    }

    NativeThread::~NativeThread()
    {
        if (!m_Joined)
        {
            pthread_detach(m_Thread);
        }
    }

    void NativeThread::Join()
    {
        pthread_join(m_Thread, nullptr);
        m_Joined = true;
    }

    bool NativeThread::SetPriority(ThreadPriority t_Priority, uint64 t_NativeId)
    {
        return SetNice(t_Priority, t_NativeId);
    }

    void NativeThread::SetCurrentName(StringView t_Name)
    {
        // The kernel limit is 16 bytes, with the terminator
        const String Short(t_Name.substr(0, 15));
        pthread_setname_np(pthread_self(), Short.c_str());
    }

    bool NativeThread::SetCurrentPriority(ThreadPriority t_Priority)
    {
        return SetNice(t_Priority, CurrentNativeId());
    }

    bool NativeThread::SetCurrentAffinity(uint64 t_Mask)
    {
        if (t_Mask == 0)
        {
            return true;
        }

        cpu_set_t Set;
        CPU_ZERO(&Set);
        for (int Cpu = 0; Cpu < 64 && Cpu < CPU_SETSIZE; ++Cpu)
        {
            if ((t_Mask >> Cpu) & 1u)
            {
                CPU_SET(Cpu, &Set);
            }
        }
        return pthread_setaffinity_np(pthread_self(), sizeof(Set), &Set) == 0;
    }

    uint64 NativeThread::CurrentNativeId()
    {
        return static_cast<uint64>(syscall(SYS_gettid));
    }
} // namespace Fling

#endif // !FLING_WINDOWS
