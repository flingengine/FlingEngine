#pragma once

#include "Frustum.h"
#include "GraphicsAPI.h"

namespace Fling
{
    /**
     * The view volume that the renderer culled with on its last frame, for Graphics.PublishSnapshot.
     * The snapshot system runs before the renderer draws, so it has no view of its own: it
     * uses this one to leave the blend out for a mesh that is far outside it (a blend costs
     * a slerp and a matrix for each moving mesh each frame).
     *
     * The view is one frame old. A mesh that comes into view that frame is drawn at the
     * latest tick's place instead of between the two ticks, for that frame only. A mesh is
     * never left out of the draw because of this. Both calls are for the main thread, where the
     * frame systems and the renderer run.
     */
    class PreCullView
    {
    public:
        /** The view of the frame that was just culled, or nullptr when it did no culling (no view is known). */
        static GRAPHICS_API void Set(const Frustum* t_View);

        /** The view, or nullptr when none is known: then nothing is left out. */
        static GRAPHICS_API const Frustum* Get();

        /**
         * True if a mesh at t_Position, up to t_Radius around it, is outside t_View by more than
         * the radius on any side: a sphere test, loose on purpose.
         */
        static bool IsFarOutside(const Frustum& t_View, const glm::vec3& t_Position, const float t_Radius)
        {
            for (const glm::vec4& Plane : t_View.Planes)
            {
                if (glm::dot(glm::vec3(Plane), t_Position) + Plane.w < -t_Radius)
                {
                    return true;
                }
            }
            return false;
        }
    };
} // namespace Fling
