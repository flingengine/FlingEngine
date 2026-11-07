#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

#include <vector>

namespace Fling
{
    /** A request from gameplay that only the application can carry out. */
    enum class AppRequest : uint8
    {
        /** Quit the game. In editor play this stops play instead (World::SetGameQuitStopsPlay). */
        Quit,
        /** Show the mouse cursor if it is hidden, or hide it if it is shown. */
        ToggleCursor,
    };

    /**
     * Queue component: requests to the application. Systems append with
     * Append<AppRequests>. The world takes them after the fixed ticks of each frame:
     * it handles Quit itself and passes the rest to the application (World::TakeAppRequests).
     */
    struct AppRequests
    {
        static constexpr ComponentInfo Component{
            .Name = "AppRequests", .Kind = ComponentKind::Queue, .Transient = true};

        std::vector<AppRequest> Requests;
    };
} // namespace Fling
