#pragma once

#include "Ecs/SystemBase.h"
#include "MotionProbe.h"
#include "RenderSnapshot.h"

namespace Sandbox
{
    /**
     * Measures how much the drawn meshes turn from one frame to the next (MotionProbe), from
     * the RenderSnapshot the renderer gets, and logs it when play ends:
     * "Sandbox: rendered turn over N frames, ... even yes, repeated frames 0".
     */
    class MotionProbeSystem final
        : public Fling::System<MotionProbeSystem,
                               Fling::Access<Fling::Read<Fling::RenderSnapshot>, Fling::Write<MotionProbe>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.MotionProbe", .In = Fling::Order::Render};

        void OnPlayBegin(Context& ctx) const;

        void Update(Context& ctx) const;

        void OnPlayEnd(Context& ctx) const;
    };
} // namespace Sandbox
