#include "Platform.h"

#if FLING_WINDOWS

#include "Misc/WindowsStrings.h"
#include "Strings/String.h"
#include "Threading/NativeThread.h"

#include <memory>


namespace Fling
{
    namespace
    {
        struct StartInfo
        {
            NativeThread::EntryFn Entry;
            void* Arg;
        };

        DWORD WINAPI Trampoline(LPVOID t_Info)
        {
            const std::unique_ptr<StartInfo> Info(static_cast<StartInfo*>(t_Info));
            Info->Entry(Info->Arg);
            return 0;
        }

        int WindowsPriority(ThreadPriority t_Priority)
        {
            switch (t_Priority)
            {
            case ThreadPriority::Lowest:
                return THREAD_PRIORITY_LOWEST;
            case ThreadPriority::BelowNormal:
                return THREAD_PRIORITY_BELOW_NORMAL;
            case ThreadPriority::Normal:
                return THREAD_PRIORITY_NORMAL;
            case ThreadPriority::AboveNormal:
                return THREAD_PRIORITY_ABOVE_NORMAL;
            case ThreadPriority::Highest:
                return THREAD_PRIORITY_HIGHEST;
            case ThreadPriority::TimeCritical:
                return THREAD_PRIORITY_TIME_CRITICAL;
            }
            return THREAD_PRIORITY_NORMAL;
        }

        // SetThreadDescription is in Windows 10 1607 and later. MinGW headers can
        // be older, so get it from kernel32 at runtime.
        using SetThreadDescriptionFn = HRESULT(WINAPI*)(HANDLE, PCWSTR);

        SetThreadDescriptionFn FindSetThreadDescription()
        {
            const HMODULE Kernel = GetModuleHandleW(L"kernel32.dll");
            if (Kernel == nullptr)
            {
                return nullptr;
            }
            const FARPROC Proc = GetProcAddress(Kernel, "SetThreadDescription");
            // Through void(*)(void) so that GCC's -Wcast-function-type accepts the cast
            return reinterpret_cast<SetThreadDescriptionFn>(reinterpret_cast<void (*)(void)>(Proc));
        }
    } // namespace

    std::unique_ptr<NativeThread> NativeThread::Start(EntryFn t_Entry, void* t_Arg, uint32 t_StackSize)
    {
        std::unique_ptr<NativeThread> Native(new NativeThread());
        auto Info = std::make_unique<StartInfo>(StartInfo{t_Entry, t_Arg});

        // STACK_SIZE_PARAM_IS_A_RESERVATION: t_StackSize is the reserve, not the commit
        const DWORD Flags = t_StackSize > 0 ? STACK_SIZE_PARAM_IS_A_RESERVATION : 0;
        Native->m_Handle = CreateThread(nullptr, t_StackSize, &Trampoline, Info.get(), Flags, nullptr);
        if (Native->m_Handle == nullptr)
        {
            return nullptr;
        }
        // The new thread owns Info now
        Info.release();
        return Native;
    }

    NativeThread::~NativeThread()
    {
        // Closing the handle detaches a thread that was not joined
        if (m_Handle != nullptr)
        {
            CloseHandle(m_Handle);
        }
    }

    void NativeThread::Join()
    {
        WaitForSingleObject(m_Handle, INFINITE);
        CloseHandle(m_Handle);
        m_Handle = nullptr;
        m_Joined = true;
    }

    bool NativeThread::SetPriority(ThreadPriority t_Priority, uint64)
    {
        return SetThreadPriority(m_Handle, WindowsPriority(t_Priority)) != 0;
    }

    void NativeThread::SetCurrentName(StringView t_Name)
    {
        static const SetThreadDescriptionFn SetDescription = FindSetThreadDescription();
        if (SetDescription == nullptr)
        {
            return;
        }
        const std::wstring Wide = Utf8ToWide(t_Name);
        SetDescription(GetCurrentThread(), Wide.c_str());
    }

    bool NativeThread::SetCurrentPriority(ThreadPriority t_Priority)
    {
        return SetThreadPriority(GetCurrentThread(), WindowsPriority(t_Priority)) != 0;
    }

    bool NativeThread::SetCurrentAffinity(uint64 t_Mask)
    {
        if (t_Mask == 0)
        {
            return true;
        }
        return SetThreadAffinityMask(GetCurrentThread(), static_cast<DWORD_PTR>(t_Mask)) != 0;
    }

    uint64 NativeThread::CurrentNativeId()
    {
        return static_cast<uint64>(GetCurrentThreadId());
    }
} // namespace Fling

#endif // FLING_WINDOWS
