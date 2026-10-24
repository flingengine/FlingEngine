#pragma once

#include "FlingTypes.h"

namespace Fling {
    /**
     * Caps for deferred directional and point lights.
     * Sandbox uses MaxPointLights when it spawns the light grid.
     */
    struct DeferredLightSettings {
        /** Dir Lights */
        static const uint32 MaxDirectionalLights = 8;

        /** Point Lights */
        static const uint32 MaxPointLights = 128;
    };
} // namespace Fling
