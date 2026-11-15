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
     * Queue component: requests to the application. Fixed systems append with
     * Append<AppRequests>, outside the resimulated groups (ScheduleErrorKind::ResimAppendsOutput).
     * After each tick the world moves them into the outbox, stamped with the tick
     * (TickOutboxEntry); the main thread applies each entry once: the world handles Quit
     * itself and passes the rest to the application (World::TakeAppRequests).
     */
    struct AppRequests
    {
        static constexpr ComponentInfo Component{
            .Name = "AppRequests", .Kind = ComponentKind::Queue, .Transient = true};

        std::vector<AppRequest> Requests;
    };
} // namespace Fling
