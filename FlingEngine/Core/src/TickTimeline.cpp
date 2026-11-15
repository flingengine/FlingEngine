#include "TickTimeline.h"

#include <cassert>

namespace Fling
{
    TickTimeline::TickTimeline(const int64 t_StepNs) : m_StepNs(t_StepNs)
    {
        assert(t_StepNs > 0);
    }

    void TickTimeline::Start(const int64 t_StartNs)
    {
        m_BaseTick = m_NextTick;
        m_BaseNs = t_StartNs;
        m_Started = true;
    }

    TickWindow TickTimeline::NextWindow() const
    {
        const int64 Start = StartOf(m_NextTick);
        return TickWindow{Start, Start + m_StepNs, TickTrigger::Request};
    }

    bool TickTimeline::IsDue(const int64 t_NowNs) const
    {
        return m_Started && !m_Paused && t_NowNs >= StartOf(m_NextTick) + m_StepNs;
    }

    TickWindow TickTimeline::Advance(const TickTrigger t_Trigger)
    {
        TickWindow Window = NextWindow();
        Window.Trigger = t_Trigger;
        ++m_NextTick;
        return Window;
    }

    uint64 TickTimeline::DropBacklog(const int64 t_NowNs)
    {
        if (!IsDue(t_NowNs))
        {
            return 0;
        }
        // The window that holds now keeps its phase: the next tick starts at the last
        // tick boundary at or before now.
        const int64 NextStart = StartOf(m_NextTick);
        const uint64 Dropped = static_cast<uint64>((t_NowNs - NextStart) / m_StepNs);
        m_BaseNs = NextStart + static_cast<int64>(Dropped) * m_StepNs;
        m_BaseTick = m_NextTick;
        return Dropped;
    }

    void TickTimeline::SetStep(const int64 t_StepNs)
    {
        assert(t_StepNs > 0);
        m_BaseNs = StartOf(m_NextTick);
        m_BaseTick = m_NextTick;
        m_StepNs = t_StepNs;
    }

    void TickTimeline::Pause(const int64 t_NowNs)
    {
        if (!m_Paused)
        {
            m_Paused = true;
            m_PausedAtNs = t_NowNs;
        }
    }

    void TickTimeline::Resume(const int64 t_NowNs)
    {
        if (!m_Paused)
        {
            return;
        }
        m_Paused = false;
        if (m_Started && t_NowNs > m_PausedAtNs)
        {
            const int64 NextStart = StartOf(m_NextTick);
            m_BaseNs = NextStart + (t_NowNs - m_PausedAtNs);
            m_BaseTick = m_NextTick;
        }
    }
} // namespace Fling
