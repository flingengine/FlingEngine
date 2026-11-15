#pragma once

#include "Ecs/Component.h"
#include "FlingMath.h"

#include <cstddef>

namespace Fling
{
    /**
     * Simple representation of a directional light for Fling. Needs to be 16 bytes aligned
     *          for Vulkan
     */
    struct alignas(16) DirectionalLight
    {
        static constexpr ComponentInfo Component{.Name = "DirectionalLight"};

        alignas(16) glm::vec4 DiffuseColor{1.0f};
        alignas(16) glm::vec4 Direction{1.0f, -1.0f, -0.5f, 1.0f};
        alignas(4) float Intensity = 1.0f;
    };
    // The std140 layout of the shaders' DirLight: the same bytes are copied into LightingUbo
    static_assert(offsetof(DirectionalLight, Direction) == 16 && offsetof(DirectionalLight, Intensity) == 32 &&
                  sizeof(DirectionalLight) == 48);
} // namespace Fling
