#pragma once

#include "FlingMath.h"
#include "FlingTypes.h"
#include "GraphicsAPI.h"

#include <span>

namespace Fling
{
    /** An axis-aligned box in a mesh's own space. A box with Min above Max on any axis is empty (not known). */
    struct Aabb
    {
        glm::vec3 Min{1.0f};
        glm::vec3 Max{-1.0f};

        /** False for an empty box: nothing was added, so the bounds are not known. */
        bool IsValid() const { return Min.x <= Max.x && Min.y <= Max.y && Min.z <= Max.z; }

        /** Grow the box to hold t_Point. Adding to an empty box makes the box of that point alone. */
        GRAPHICS_API void Add(const glm::vec3& t_Point);

        /** The box around every point. Empty when there are none. */
        GRAPHICS_API static Aabb Of(std::span<const glm::vec3> t_Points);
    };

    /**
     * The six planes of a camera's view volume, in world space, for culling. Each plane is
     * (n, d) with n pointing in: a point p is on the inside of the plane when dot(n, p) + d >= 0.
     *
     * The planes are those of the clip volume that Vulkan keeps: -w <= x <= w, -w <= y <= w and
     * 0 <= z <= w. A y flip in the projection (Vulkan's y points down) only swaps the top and
     * bottom planes, so the set is the same with or without it.
     */
    struct Frustum
    {
        enum Side : uint32
        {
            Left = 0,
            Right,
            Bottom,
            Top,
            Near,
            Far,
            Count
        };

        glm::vec4 Planes[Side::Count] = {};

        /**
         * The planes of t_ViewProjection (projection * view), the matrix that takes a world
         * position to clip space. Each plane is normalized, so its distance is in world units.
         */
        GRAPHICS_API static Frustum FromViewProjection(const glm::mat4& t_ViewProjection);

        /**
         * True if any part of the box t_Local, placed in the world by t_World, can be inside the
         * view volume. Conservative: it can say true for a box that is just outside a corner of
         * the volume, never false for a box that is inside. An empty box is always visible.
         */
        GRAPHICS_API bool Intersects(const Aabb& t_Local, const glm::mat4& t_World) const;

        /** True if any part of the world space box with centre t_Centre and half size t_Extents can be inside. */
        GRAPHICS_API bool IntersectsWorldBox(const glm::vec3& t_Centre, const glm::vec3& t_Extents) const;
    };
} // namespace Fling
