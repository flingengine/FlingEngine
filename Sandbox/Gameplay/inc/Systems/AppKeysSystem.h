#pragma once

#include "Components/AppRequests.h"
#include "Components/InputState.h"
#include "Ecs/SystemBase.h"

namespace Sandbox
{
    /**
     * Turns the application keys into AppRequests: Escape (or View) quits, and M
     * (or Menu) shows or hides the mouse cursor. In editor play, quit stops play.
     * In Order::Input, which a resimulation does not run, so a replayed tick does not ask
     * again (ScheduleErrorKind::ResimAppendsOutput). The world sends the requests to the
     * main thread stamped with this tick (TickOutboxEntry).
     */
    class AppKeysSystem final
        : public Fling::System<AppKeysSystem,
                               Fling::Access<Fling::Read<Fling::InputState>, Fling::Append<Fling::AppRequests>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.AppKeys", .In = Fling::Order::Input};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Sandbox
