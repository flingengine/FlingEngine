#pragma once

#include "Components/Transform.h"
#include "Ecs/Component.h"
#include "FlingMath.h"
#include "Platform.h"

#include <cstddef>

namespace Fling
{
    /**
     * Simple representation of a point light in Light Vox. The colors and position have
     *          to be glm::vec4's because of shader alignment things
     */
    struct alignas(16) PointLight
    {
    public:
        static constexpr ComponentInfo Component{.Name = "PointLight"};

        /** A light needs a place in the world */
        using Requires = TypeList<Transform>;

        /**
         * Diffuse color of this point light, RBA on a scale of 0.0 to 1.0
         */
        glm::vec4 DiffuseColor{1.0f};

        /**
         * The world position, for the shader. Graphics.PublishSnapshot sets it in the copy it
         * gives the renderer, from the Transform. In the component it stays as it was made.
         */
        glm::vec4 Pos{0.0f};

        alignas(4) float Intensity = 10.0f;
        alignas(4) float Range = 5.0f;

        FORCEINLINE void SetPos(const glm::vec4& t_Pos) { Pos = t_Pos; }
    };
    // The std140 layout of the shaders' PointLight: the same bytes are copied into LightingUbo
    static_assert(offsetof(PointLight, Pos) == 16 && offsetof(PointLight, Intensity) == 32 &&
                  offsetof(PointLight, Range) == 36 && sizeof(PointLight) == 48);
} // namespace Fling
