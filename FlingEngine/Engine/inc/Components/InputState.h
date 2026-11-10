#pragma once

#include "Ecs/Component.h"
#include "Input/TickInput.h"

namespace Fling
{
    /**
     * Singleton component: the input of this simulation tick. The Engine.Input system
     * writes it in the Input phase. Gameplay systems read it with Read<InputState>,
     * which also orders them after Engine.Input. Presses and releases are counts for
     * this tick, so check WasPressed, not only IsHeld.
     */
    struct InputState
    {
        static constexpr ComponentInfo Component{
            .Name = "InputState", .Kind = ComponentKind::Singleton, .Transient = true};

        TickInput Tick;

        bool IsHeld(InputKey t_Key) const { return Tick.IsHeld(t_Key); }

        bool WasPressed(InputKey t_Key) const { return Tick.WasPressed(t_Key); }

        bool WasReleased(InputKey t_Key) const { return Tick.WasReleased(t_Key); }

        float GetAxisValue(InputKey t_Key) const { return Tick.GetAxisValue(t_Key); }
    };
} // namespace Fling
