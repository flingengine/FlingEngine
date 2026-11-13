#pragma once

#include "Components/InputSnapshot.h"
#include "Components/InputSource.h"
#include "Ecs/SystemBase.h"

namespace Fling
{
    /**
     * Builds the InputSnapshot singleton once for each fixed tick: it takes the input
     * messages that no earlier tick took, and merges the state of every device. It owns
     * InputSnapshot and InputSource, and it is the only system that reads the input
     * message queue. Every other fixed system reads InputSnapshot.
     *
     * A frame that runs no tick keeps its messages for the next tick that runs, and the
     * second tick of a frame gets no edges again. Dead zones and action mapping go here
     * later, as pure functions (Plans/improve-input.md).
     */
    class InputSystem final : public System<InputSystem, Access<Write<InputSource>, Write<InputSnapshot>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Engine.Input", .Phase = Phases::Input, .RunIn = WorldRole::All};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Fling
