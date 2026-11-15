#pragma once

#include "CameraStatus.h"
#include "Ecs/SystemBase.h"
#include "LookCamera.h"
#include "Stasis.h"

namespace Sandbox
{
    /**
     * Writes CameraStatus each frame from the published LookCamera and its Stasis, for the
     * UI: stasis (with the time left), frozen, or free. It reads only published state, so
     * a press of F shows here from the first frame after the tick that consumed it.
     */
    class CameraStatusSystem final
        : public Fling::System<CameraStatusSystem,
                               Fling::Access<Fling::Read<LookCamera>, Fling::Read<Stasis>, Fling::Write<CameraStatus>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.CameraStatus", .In = Fling::Order::Presentation};

        void Update(Context& ctx) const;
    };
} // namespace Sandbox
