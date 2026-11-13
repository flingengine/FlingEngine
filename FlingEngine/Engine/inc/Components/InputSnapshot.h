#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"
#include "Input/InputKey.h"
#include "Input/InputKeyState.h"

#include <utility>
#include <vector>

namespace Fling
{
    /**
     * Singleton component: the input of this simulation tick, merged over every device.
     * The Engine.Input system writes it once in the Input phase, and it does not change
     * for the rest of the tick. Gameplay systems read it with Read<InputSnapshot>, which
     * also orders them after Engine.Input. It is the only input that a fixed system reads.
     *
     * Presses and releases are counts, so a press and a release inside one tick give
     * PressCount 1, ReleaseCount 1, and IsHeld false: no press is lost. Check WasPressed,
     * not only IsHeld.
     */
    struct InputSnapshot
    {
        static constexpr ComponentInfo Component{
            .Name = "InputSnapshot", .Kind = ComponentKind::Singleton, .Transient = true};

        /** One entry for each key that has state (held, an edge, or an axis value), sorted by key name. */
        std::vector<std::pair<InputKey, InputKeyState>> Keys;

        /** Mouse movement this tick, in integer counts. */
        int32 MouseDeltaX = 0;
        int32 MouseDeltaY = 0;

        /** The merged state of one key: held on any device, edge counts summed, the largest axis value. */
        InputKeyState GetKeyState(InputKey t_Key) const
        {
            for (const auto& Entry : Keys)
            {
                if (Entry.first == t_Key)
                {
                    return Entry.second;
                }
            }
            return InputKeyState{};
        }

        bool IsHeld(InputKey t_Key) const { return GetKeyState(t_Key).IsHeld; }

        bool WasPressed(InputKey t_Key) const { return GetKeyState(t_Key).WasPressed(); }

        bool WasReleased(InputKey t_Key) const { return GetKeyState(t_Key).WasReleased(); }

        float GetAxisValue(InputKey t_Key) const { return GetKeyState(t_Key).Value; }
    };
} // namespace Fling
