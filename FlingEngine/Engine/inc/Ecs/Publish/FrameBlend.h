#pragma once

#include "Ecs/Publish/HeldValues.h"
#include "Ecs/Publish/TickPacket.h"
#include "Ecs/TickStamp.h"
#include "FlingTypes.h"

#include <optional>
#include <vector>

namespace Fling
{
    /**
     * The latest two published ticks that a presentation link shows (PresentationLink::ShowLatest):
     * tick N, the newest tick the frame storage holds, and tick N - 1 when the link has it,
     * each with the tables that hold its values of the blended types. The tables belong to
     * the link and do not change until its next ShowLatest.
     */
    struct ShownTicks
    {
        TickStamp Latest;
        const std::vector<PacketTable>* LatestTables = nullptr;
        std::optional<TickStamp> Previous;
        const std::vector<PacketTable>* PreviousTables = nullptr;
    };

    /** What one Play frame draws (FrameBlend::Advance): BlendFrom, BlendTo and the factor between them. */
    struct BlendFrame
    {
        TickStamp FromStamp;
        TickStamp ToStamp;
        const HeldValues* From = nullptr;
        const HeldValues* To = nullptr;
        float Alpha = 1.0f;
    };

    /**
     * The renderer's interpolation state, on the frame side only (Docs/time-and-ticks.md,
     * "The blend factor"): the latest two published ticks it received, BlendFrom (tick
     * N - 1) and BlendTo (tick N), and the point it drew between them. The world owns one,
     * gives it what the link shows at each ShowLatest (Show), and moves it on once per Play
     * frame (Advance). Nothing on the simulation side reads it.
     *
     * The rule, the one place that computes BlendAlpha: the drawn point is a tick number
     * with a fraction. Each Play frame moves it on by the frame's time over the tick length
     * (BlendTo's window), on the renderer's own clock, then keeps it between BlendFrom and
     * BlendTo. BlendAlpha is how far past BlendFrom it is:
     *
     *     Drawn = clamp(DrawnLastFrame + FrameDeltaNs / TickLengthNs, N - 1, N)
     *     BlendAlpha = Drawn - (N - 1)
     *
     * So the motion is continuous while the ticks come one tick length apart; a late tick
     * holds the frames at BlendTo (never past it: no extrapolation); and when the frame
     * side receives several new ticks at once (a slower renderer, a wake that ran several
     * ticks), the point moves up to BlendFrom, a skip of the ticks it did not draw, never a
     * faster blend. Nothing waits. The first frame of a link starts the point at BlendFrom.
     * With one state (the start of play), with interpolation off, or for a tick with no
     * clock window, the frame draws BlendTo (factor 1) and the point is there.
     */
    class FrameBlend
    {
    public:
        /** Hold the values of these types (PublishedSet::Blended). Forgets the states. */
        void SetTypes(const std::vector<Detail::ComponentId>& types);

        /** Blend between the two ticks (on, the default), or draw BlendTo (off). */
        void SetInterpolation(bool on) { m_Interpolation = on; }

        bool Interpolation() const { return m_Interpolation; }

        /** Forget the states and the drawn point (a new presentation link). */
        void Reset();

        /** Take the ticks a link shows. Ticks it has already taken change nothing. */
        void Show(const ShownTicks& ticks);

        /** The ticks held: 0 before the first Show, 1 without tick N - 1, else 2. */
        uint32 Count() const { return m_Count; }

        /** One Play frame at frameNs (the frame clock): move the drawn point on. Null before the first Show. */
        const BlendFrame* Advance(int64 frameNs);

    private:
        HeldValues m_From;
        HeldValues m_To;
        TickStamp m_FromStamp;
        TickStamp m_ToStamp;
        uint32 m_Count = 0;
        bool m_Interpolation = true;

        /** The point the last Play frame drew: BlendTo's tick, the factor, and the frame time. */
        bool m_HasDrawn = false;
        uint64 m_DrawnTo = 0;
        double m_DrawnAlpha = 1.0;
        int64 m_DrawnNs = 0;

        BlendFrame m_Frame;
    };
} // namespace Fling
