#include "Ecs/Publish/FrameBlend.h"
#include "MathUtils.h"

#include <utility>

namespace Fling
{
    void FrameBlend::SetTypes(const std::vector<Detail::ComponentId>& types)
    {
        m_From = HeldValues(types);
        m_To = HeldValues(types);
        Reset();
    }

    void FrameBlend::Reset()
    {
        m_From.Clear();
        m_To.Clear();
        m_Count = 0;
        m_HasDrawn = false;
    }

    void FrameBlend::Show(const ShownTicks& ticks)
    {
        if (m_Count > 0 && ticks.Latest.Tick == m_ToStamp.Tick)
        {
            return;
        }
        if (ticks.Previous && ticks.PreviousTables != nullptr)
        {
            if (m_Count > 0 && ticks.Previous->Tick == m_ToStamp.Tick)
            {
                // The new BlendFrom is a copy of the BlendTo held: keep its index
                std::swap(m_From, m_To);
                m_From.ShowSameRows(*ticks.PreviousTables);
            }
            else
            {
                m_From.Show(*ticks.PreviousTables);
            }
            m_FromStamp = *ticks.Previous;
            m_Count = 2;
        }
        else
        {
            m_From.Clear();
            m_Count = 1;
        }
        m_ToStamp = ticks.Latest;
        m_To.Show(*ticks.LatestTables);
    }

    const BlendFrame* FrameBlend::Advance(const int64 frameNs)
    {
        if (m_Count == 0)
        {
            return nullptr;
        }
        const int64 lengthNs = m_ToStamp.Window.EndNs - m_ToStamp.Window.StartNs;
        const bool blends =
            m_Interpolation && m_Count == 2 && m_ToStamp.Window.Trigger == TickTrigger::Clock && lengthNs > 0;
        // Before any frame of this link the point starts at BlendFrom; without a blend it is at BlendTo
        double alpha = blends ? 0.0 : 1.0;
        if (blends && m_HasDrawn)
        {
            // The point drawn last frame, in ticks past this frame's BlendFrom, moved on by this frame's time
            const double lastPoint = static_cast<double>(static_cast<int64>(m_DrawnTo - m_ToStamp.Tick)) + m_DrawnAlpha;
            const double moved =
                static_cast<double>(Math::Max<int64>(frameNs - m_DrawnNs, 0)) / static_cast<double>(lengthNs);
            alpha = Math::Clamp(lastPoint + moved, 0.0, 1.0);
        }
        m_HasDrawn = true;
        m_DrawnTo = m_ToStamp.Tick;
        m_DrawnAlpha = alpha;
        m_DrawnNs = frameNs;

        m_Frame.ToStamp = m_ToStamp;
        m_Frame.To = &m_To;
        m_Frame.FromStamp = blends ? m_FromStamp : m_ToStamp;
        m_Frame.From = blends ? &m_From : &m_To;
        m_Frame.Alpha = static_cast<float>(alpha);
        return &m_Frame;
    }
} // namespace Fling
