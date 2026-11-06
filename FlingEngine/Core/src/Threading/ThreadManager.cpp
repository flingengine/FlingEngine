#include "Threading/Mutex.h"
#include "Threading/ThreadManager.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <map>

namespace Fling
{
    namespace
    {
        // #TODO: Replace the mutex and map with a lock-free slot table, so that the
        // crash handler can read thread names. See Plans/lockless-threading.md
        struct Registry
        {
            Mutex ThreadsMutex;
            std::map<ThreadId, ThreadInfo> Threads FLING_GUARDED_BY(ThreadsMutex);
            std::atomic<ThreadId> NextId{GameThreadId + 1};
        };

        // In this .cpp so that every module sees the one registry in Core
        Registry& GetRegistry()
        {
            static Registry Instance;
            return Instance;
        }
    } // namespace

    namespace ThreadManager
    {
        ThreadId AllocateId()
        {
            return GetRegistry().NextId.fetch_add(1, std::memory_order_relaxed);
        }

        void Register(ThreadId t_Id, std::string_view t_Name, uint64 t_NativeId)
        {
            Registry& Reg = GetRegistry();
            ScopedLock Lock(Reg.ThreadsMutex);
            Reg.Threads[t_Id] = ThreadInfo{t_Id, std::string(t_Name), t_NativeId};
        }

        void Unregister(ThreadId t_Id)
        {
            Registry& Reg = GetRegistry();
            ScopedLock Lock(Reg.ThreadsMutex);
            Reg.Threads.erase(t_Id);
        }

        std::string GetThreadName(ThreadId t_Id)
        {
            Registry& Reg = GetRegistry();
            ScopedLock Lock(Reg.ThreadsMutex);
            const auto It = Reg.Threads.find(t_Id);
            return It != Reg.Threads.end() ? It->second.Name : std::string();
        }

        std::vector<ThreadInfo> GetThreads()
        {
            Registry& Reg = GetRegistry();
            ScopedLock Lock(Reg.ThreadsMutex);
            std::vector<ThreadInfo> Result;
            Result.reserve(Reg.Threads.size());
            for (const auto& Pair : Reg.Threads)
            {
                Result.push_back(Pair.second);
            }
            return Result;
        }
    } // namespace ThreadManager
} // namespace Fling
