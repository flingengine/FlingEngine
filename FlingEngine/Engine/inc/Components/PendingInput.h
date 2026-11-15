#pragma once

#include "Ecs/Component.h"
#include "Input/InputSlice.h"

namespace Fling
{
    /**
     * Singleton component: the input that arrived after the cut of the tick the frame
     * shows (RealFrameTime::Latest), up to now. The runner makes it again from scratch
     * before the frame systems of each frame run (SystemRunner::SetPendingInputSource), so
     * it holds no running state: when a tick consumes the input, the next frame leaves it
     * out. No system can write it; a frame system reads it with Read<PendingInput>.
     *
     * It is an InputSlice, the same type as a tick's input (TickInput::Slice), made by the
     * same code. So a game rule that takes an InputSlice runs on both sides: use it to show
     * input before the simulation sees it (late camera look, UI hover). It is a
     * Presentation component, so a fixed rate system cannot read it.
     */
    struct PendingInput : InputSlice
    {
        static constexpr ComponentInfo Component{.Name = "PendingInput",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
    };
} // namespace Fling
