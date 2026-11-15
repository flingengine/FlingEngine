#pragma once

#include "Ecs/Component.h"
#include "Input/TickInput.h"

namespace Fling
{
    /**
     * Singleton component: the input of this simulation tick. The runner writes it before
     * the fixed systems of each tick run (SystemRunner::SetTickInputSource), and no system
     * can write it, so a fixed system gets input only by declaring Read<InputState>. The
     * tick's cut is in its TickStamp, not here. Presses and releases are counts for this
     * tick, so check WasPressed, not only IsHeld. A rollback does not restore it: a
     * resimulation writes each replayed tick's recorded input into it
     * (Schedule::ResimInputs lists it, from the runner).
     */
    struct InputState
    {
        static constexpr ComponentInfo Component{.Name = "InputState",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Rebuild};

        TickInput Tick;

        bool IsHeld(InputKey t_Key) const { return Tick.IsHeld(t_Key); }

        bool WasPressed(InputKey t_Key) const { return Tick.WasPressed(t_Key); }

        bool WasReleased(InputKey t_Key) const { return Tick.WasReleased(t_Key); }

        float GetAxisValue(InputKey t_Key) const { return Tick.GetAxisValue(t_Key); }
    };
} // namespace Fling
