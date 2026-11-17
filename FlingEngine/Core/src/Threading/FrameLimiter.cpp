#include "MathUtils.h"
#include "PlatformTime.h"
#include "Threading/CpuRelax.h"
#include "Threading/FrameLimiter.h"
#include "Threading/SleepTimer.h"

#include <chrono>

namespace Fling
{
    namespace
    {
        using SteadyClock = std::chrono::steady_clock;

        int64 SteadyNowNs()
        {
            return std::chrono::duration_cast<std::chrono::nanoseconds>(SteadyClock::now().time_since_epoch()).count();
        }
    } // namespace

    FrameLimiter::FrameLimiter() : m_Timer(std::make_unique<SleepTimer>()), m_SpinNs(DefaultSpinNs)
    {
        SleepTimer* Timer = m_Timer.get();
        m_Now = &SteadyNowNs;
        m_Sleep = [Timer](const int64 t_UntilNs)
        {
            Timer->SleepUntil(SteadyClock::time_point(std::chrono::nanoseconds(t_UntilNs)));
        };
    }

    FrameLimiter::FrameLimiter(NowFunction t_Now, SleepFunction t_Sleep, const int64 t_SpinNs)
        : m_Now(std::move(t_Now)), m_Sleep(std::move(t_Sleep)), m_SpinNs(Math::Max<int64>(t_SpinNs, 0))
    {
    }

    FrameLimiter::~FrameLimiter() = default;

    void FrameLimiter::SetMaxFps(const uint32 t_Fps)
    {
        if (t_Fps == m_MaxFps)
        {
            return;
        }
        m_MaxFps = t_Fps;
        m_PeriodNs = PlatformTime::PeriodNs(t_Fps);
        m_Started = false;
    }

    int64 FrameLimiter::GetSpinNs() const
    {
        return Math::Min(m_SpinNs, m_PeriodNs / 2);
    }

    bool FrameLimiter::WillWait() const
    {
        return m_PeriodNs > 0 && m_Started && m_Now() < m_NextNs;
    }

    void FrameLimiter::WaitForNextFrame()
    {
        if (m_PeriodNs <= 0)
        {
            return;
        }

        int64 Now = m_Now();
        if (!m_Started)
        {
            m_Started = true;
            m_NextNs = Now + m_PeriodNs;
            return;
        }

        if (Now < m_NextNs)
        {
            const int64 WakeAt = m_NextNs - GetSpinNs();
            if (Now < WakeAt)
            {
                m_Sleep(WakeAt);
            }
            while (m_Now() < m_NextNs)
            {
                CpuRelax();
            }
            m_NextNs += m_PeriodNs;
            return;
        }

        if (Now - m_NextNs > m_PeriodNs / 2)
        {
            // Too late to keep the schedule: start it again, so the next frames do not run
            // back to back to catch up
            m_NextNs = Now + m_PeriodNs;
            ++m_Restarts;
            return;
        }
        // A little late: no wait, and the next frame keeps its deadline (it is shorter)
        m_NextNs += m_PeriodNs;
    }
} // namespace Fling
