#include "FlingPaths.h"
#include "Logger.h"
#include "Screenshot.h"
#include "Threading/ThreadQueue.h"

#include <atomic>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <utility>

namespace Fling
{
    namespace
    {
        /** Any thread to the main thread: the requests waiting for a frame. */
        ThreadQueue<Screenshot::Pending>& PendingQueue()
        {
            static ThreadQueue<Screenshot::Pending> s_Queue;
            return s_Queue;
        }

        /** Makes DefaultPath names unique within one second. */
        std::atomic<uint32> s_DefaultPathCount{0};
    } // namespace

    String Screenshot::DefaultPath()
    {
        const std::time_t Now = std::time(nullptr);
        std::tm LocalTime = {};
#if defined(_WIN32)
        localtime_s(&LocalTime, &Now);
#else
        localtime_r(&Now, &LocalTime);
#endif
        std::ostringstream Name;
        Name << FlingPaths::ProjectSavedDir() << "/Screenshots/Screenshot_"
             << std::put_time(&LocalTime, "%Y-%m-%d_%H-%M-%S") << "_"
             << s_DefaultPathCount.fetch_add(1, std::memory_order_relaxed) + 1 << ".png";
        return Name.str();
    }

    String Screenshot::Request(const StringView t_Path, ScreenshotCallback t_OnDone)
    {
        const String Path = t_Path.empty() ? DefaultPath() : FlingPaths::ResolveProjectPath(t_Path);
        PendingQueue().Push(Pending{Path, std::move(t_OnDone)});
        return Path;
    }

    std::vector<Screenshot::Pending> Screenshot::TakePending()
    {
        std::vector<Pending> Requests;
        PendingQueue().Drain([&Requests](Pending& t_Request) { Requests.push_back(std::move(t_Request)); });
        return Requests;
    }

    void Screenshot::Finish(Pending& t_Request, const ScreenshotResult& t_Result)
    {
        if (t_Result.Ok)
        {
            F_LOG_TRACE("Screenshot saved ({}x{}) to {}", t_Result.Width, t_Result.Height, t_Result.Path);
        }
        else
        {
            F_LOG_ERROR("Screenshot not saved to {}: {}", t_Result.Path, t_Result.Error);
        }
        if (t_Request.OnDone)
        {
            t_Request.OnDone(t_Result);
        }
    }

    void Screenshot::FailPending(const StringView t_Reason)
    {
        for (Pending& Request : TakePending())
        {
            ScreenshotResult Result;
            Result.Path = Request.Path;
            Result.Error = String(t_Reason);
            Finish(Request, Result);
        }
    }
} // namespace Fling
