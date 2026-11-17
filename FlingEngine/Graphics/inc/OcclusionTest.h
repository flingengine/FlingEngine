#pragma once

#include "FlingMath.h"
#include "FlingTypes.h"
#include "Frustum.h"
#include "GraphicsAPI.h"

#include <span>
#include <vector>

namespace Fling
{
    /**
     * The bounds of a mesh on the screen, for the occlusion test: the rectangle (in 0 to 1
     * screen space, y down like the frame buffer) and the depth of the nearest point.
     */
    struct ScreenBounds
    {
        glm::vec2 MinUv{0.0f};
        glm::vec2 MaxUv{1.0f};

        /** The smallest depth (0 near, 1 far) of the box, never below 0 */
        float NearDepth = 0.0f;

        /**
         * False when the box reaches the plane of the camera, so the box has no safe screen
         * rectangle. Such a box is never occluded.
         */
        bool Testable = false;
    };

    /**
     * A depth pyramid: level 0 is the depth image, and each texel of the next level holds the
     * farthest (largest) depth of the texels below it. A level is half the size of the one
     * before (rounded down, at least 1), and a last row or column that has an odd size covers
     * the extra source row or column. The compute shaders (Occlusion/hiz_downsample.comp and
     * Occlusion/cull.comp) do the same, so the test runs on the CPU for tests.
     */
    class DepthPyramid
    {
    public:
        /** Build every level from t_Depth, t_Width * t_Height values in rows, y down */
        GRAPHICS_API void Build(std::span<const float> t_Depth, uint32 t_Width, uint32 t_Height);

        uint32 LevelCount() const { return static_cast<uint32>(m_Levels.size()); }
        uint32 Width(const uint32 t_Level) const { return m_Levels[t_Level].Width; }
        uint32 Height(const uint32 t_Level) const { return m_Levels[t_Level].Height; }

        /** The farthest depth under one texel of a level */
        float Texel(const uint32 t_Level, const uint32 t_X, const uint32 t_Y) const
        {
            const Level& L = m_Levels[t_Level];
            return L.Texels[size_t{t_Y} * L.Width + t_X];
        }

        /** The number of levels a pyramid of this size has: down to 1 x 1 */
        GRAPHICS_API static uint32 LevelsFor(uint32 t_Width, uint32 t_Height);

        /** The size of a level after t_Size: half, at least 1 */
        static uint32 NextSize(const uint32 t_Size) { return t_Size > 1 ? t_Size >> 1 : 1; }

    private:
        struct Level
        {
            uint32 Width = 0;
            uint32 Height = 0;
            std::vector<float> Texels;
        };
        std::vector<Level> m_Levels;
    };

    namespace Occlusion
    {
        /**
         * A box is occluded when its nearest depth is farther than the farthest depth under it
         * by more than this. It keeps a mesh that fills its own box from hiding itself, because
         * the shader and the rasterizer do not round the same way.
         */
        constexpr float DepthBias = 1.0e-5f;

        /**
         * Project the box t_Local, placed by t_World, with t_ViewProjection (the matrix the
         * G-buffer shader uses). Never hides anything: a box that reaches the camera's plane
         * is Testable = false.
         */
        GRAPHICS_API ScreenBounds ProjectBounds(const Aabb& t_Local,
                                                const glm::mat4& t_World,
                                                const glm::mat4& t_ViewProjection);

        /**
         * True if the pyramid shows that every pixel of the box is hidden: the box is farther
         * than the farthest depth under its screen rectangle. Conservative: false when unsure.
         */
        GRAPHICS_API bool IsOccluded(const DepthPyramid& t_Pyramid, const ScreenBounds& t_Bounds);
    } // namespace Occlusion
} // namespace Fling
