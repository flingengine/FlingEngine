#pragma once

#include "CoreAPI.h"
#include "Strings/String.h"
#include "Threading/ThreadTypes.h"

#include <string>
#include <string_view>
#include <vector>

namespace Fling
{
    /** One entry of the thread registry. */
    struct ThreadInfo
    {
        ThreadId Id = InvalidThreadId;
        String Name;
        uint64 NativeId = 0;
    };

    /**
     * Registry of the threads that are running, for logs, crash reports, and the
     * profiler. Thread adds and removes its entries. Other code only reads them.
     *
     * The registry cannot find a Thread object. Do not use it as a global path to
     * send work to a thread. The owner of a thread gives out that path.
     */
    namespace ThreadManager
    {
        /** Return a new ID. IDs start after GameThreadId and are not used again. */
        CORE_API ThreadId AllocateId();

        /** Add t_Id, or replace its entry. */
        CORE_API void Register(ThreadId t_Id, StringView t_Name, uint64 t_NativeId);

        CORE_API void Unregister(ThreadId t_Id);

        /** The name of t_Id, or an empty string when t_Id is not running. */
        CORE_API String GetThreadName(ThreadId t_Id);

        /** A copy of every entry, in ID order. */
        CORE_API std::vector<ThreadInfo> GetThreads();
    } // namespace ThreadManager
} // namespace Fling
