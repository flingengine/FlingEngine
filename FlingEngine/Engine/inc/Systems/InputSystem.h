#pragma once

#include "Components/InputState.h"
#include "Ecs/SystemBase.h"

namespace Fling
{
    /**
     * Copies this tick's input into the InputState singleton, which it owns. It is the
     * only system that reads the tick input. Dead zones and action mapping go here later,
     * as pure functions (Plans/improve-input.md).
     */
    class InputSystem final : public System<InputSystem, Access<Write<InputState>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Engine.Input", .Phase = Phases::Input, .RunIn = WorldRole::All};

        void FixedUpdate(Context& ctx) const;
    };
} // namespace Fling
