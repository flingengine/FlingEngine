#pragma once

#include "Components/Transform.h"
#include "Ecs/Component.h"
#include "FlingMath.h"
#include "Platform.h"

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

    private:
        /** The position of this point light will be set from it's Transform component.
         * set per frame
         */
        glm::vec4 Pos{0.0f};

    public:
        alignas(4) float Intensity = 10.0f;
        alignas(4) float Range = 5.0f;

        FORCEINLINE void SetPos(const glm::vec4& t_Pos) { Pos = t_Pos; }
    };
} // namespace Fling
