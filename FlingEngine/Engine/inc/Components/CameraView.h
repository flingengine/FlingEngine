#pragma once

#include "Ecs/Component.h"

namespace Fling
{
    /**
     * Presentation singleton: the rotation to draw the camera with this frame, in radians.
     * A game's frame system writes it (and so owns it) each frame; the renderer reads it
     * from the storage it draws, and turns its camera with it in place of the debug
     * camera's rotation. With no writer, or with Active false, the renderer keeps the debug
     * camera. The engine does not know where the view comes from.
     */
    struct CameraView
    {
        static constexpr ComponentInfo Component{.Name = "CameraView",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};

        /** False: the renderer uses its debug camera. */
        bool Active = false;

        /** Turn around the world up axis. */
        float Yaw = 0.0f;

        /** Look up (positive) or down. */
        float Pitch = 0.0f;
    };
} // namespace Fling
