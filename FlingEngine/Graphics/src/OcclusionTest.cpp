#include "OcclusionTest.h"

#include <algorithm>
#include <cmath>

namespace Fling
{
    uint32 DepthPyramid::LevelsFor(const uint32 t_Width, const uint32 t_Height)
    {
        uint32 Size = std::max({t_Width, t_Height, 1u});
        uint32 Levels = 1;
        while (Size > 1)
        {
            Size >>= 1;
            ++Levels;
        }
        return Levels;
    }

    void DepthPyramid::Build(const std::span<const float> t_Depth, const uint32 t_Width, const uint32 t_Height)
    {
        m_Levels.clear();
        if (t_Width == 0 || t_Height == 0 || t_Depth.size() < size_t{t_Width} * t_Height)
        {
            return;
        }

        Level Base;
        Base.Width = t_Width;
        Base.Height = t_Height;
        Base.Texels.assign(t_Depth.begin(), t_Depth.begin() + size_t{t_Width} * t_Height);
        m_Levels.push_back(std::move(Base));

        const uint32 Count = LevelsFor(t_Width, t_Height);
        for (uint32 L = 1; L < Count; ++L)
        {
            const Level& Src = m_Levels[L - 1];
            Level Dst;
            Dst.Width = NextSize(Src.Width);
            Dst.Height = NextSize(Src.Height);
            Dst.Texels.resize(size_t{Dst.Width} * Dst.Height);
            for (uint32 y = 0; y < Dst.Height; ++y)
            {
                for (uint32 x = 0; x < Dst.Width; ++x)
                {
                    const uint32 X0 = x * 2;
                    const uint32 Y0 = y * 2;
                    const uint32 X1 = (x == Dst.Width - 1) ? Src.Width - 1 : X0 + 1;
                    const uint32 Y1 = (y == Dst.Height - 1) ? Src.Height - 1 : Y0 + 1;
                    float Farthest = 0.0f;
                    for (uint32 sy = Y0; sy <= Y1; ++sy)
                    {
                        for (uint32 sx = X0; sx <= X1; ++sx)
                        {
                            Farthest = std::max(Farthest, Src.Texels[size_t{sy} * Src.Width + sx]);
                        }
                    }
                    Dst.Texels[size_t{y} * Dst.Width + x] = Farthest;
                }
            }
            m_Levels.push_back(std::move(Dst));
        }
    }

    namespace Occlusion
    {
        ScreenBounds ProjectBounds(const Aabb& t_Local, const glm::mat4& t_World, const glm::mat4& t_ViewProjection)
        {
            ScreenBounds Result;
            if (!t_Local.IsValid())
            {
                return Result;
            }

            const glm::mat4 Mvp = t_ViewProjection * t_World;
            glm::vec2 Min{1.0e30f};
            glm::vec2 Max{-1.0e30f};
            float Nearest = 1.0e30f;
            for (uint32 i = 0; i < 8; ++i)
            {
                const glm::vec3 Corner{(i & 1u) ? t_Local.Max.x : t_Local.Min.x,
                                       (i & 2u) ? t_Local.Max.y : t_Local.Min.y,
                                       (i & 4u) ? t_Local.Max.z : t_Local.Min.z};
                const glm::vec4 Clip = Mvp * glm::vec4(Corner, 1.0f);
                if (Clip.w <= 1.0e-4f)
                {
                    return Result;
                }
                const glm::vec2 Uv = glm::vec2(Clip.x, Clip.y) / Clip.w * 0.5f + 0.5f;
                Min = glm::min(Min, Uv);
                Max = glm::max(Max, Uv);
                Nearest = std::min(Nearest, Clip.z / Clip.w);
            }

            Result.MinUv = glm::clamp(Min, glm::vec2(0.0f), glm::vec2(1.0f));
            Result.MaxUv = glm::clamp(Max, glm::vec2(0.0f), glm::vec2(1.0f));
            Result.NearDepth = std::max(Nearest, 0.0f);
            Result.Testable = true;
            return Result;
        }

        bool IsOccluded(const DepthPyramid& t_Pyramid, const ScreenBounds& t_Bounds)
        {
            if (!t_Bounds.Testable || t_Pyramid.LevelCount() == 0)
            {
                return false;
            }

            const float W = static_cast<float>(t_Pyramid.Width(0));
            const float H = static_cast<float>(t_Pyramid.Height(0));
            const int32 MaxX = static_cast<int32>(t_Pyramid.Width(0)) - 1;
            const int32 MaxY = static_cast<int32>(t_Pyramid.Height(0)) - 1;
            const int32 X0 = std::clamp(static_cast<int32>(std::floor(t_Bounds.MinUv.x * W)), 0, MaxX);
            const int32 X1 = std::clamp(static_cast<int32>(std::floor(t_Bounds.MaxUv.x * W)), 0, MaxX);
            const int32 Y0 = std::clamp(static_cast<int32>(std::floor(t_Bounds.MinUv.y * H)), 0, MaxY);
            const int32 Y1 = std::clamp(static_cast<int32>(std::floor(t_Bounds.MaxUv.y * H)), 0, MaxY);

            // The level where the rectangle covers at most 2 x 2 texels
            const int32 Span = std::max(X1 - X0, Y1 - Y0) + 1;
            uint32 Level = 0;
            while ((1 << Level) < Span && Level + 1 < t_Pyramid.LevelCount())
            {
                ++Level;
            }

            const int32 LX1 = static_cast<int32>(t_Pyramid.Width(Level)) - 1;
            const int32 LY1 = static_cast<int32>(t_Pyramid.Height(Level)) - 1;
            float Farthest = 0.0f;
            for (int32 y = std::min(Y0 >> Level, LY1); y <= std::min(Y1 >> Level, LY1); ++y)
            {
                for (int32 x = std::min(X0 >> Level, LX1); x <= std::min(X1 >> Level, LX1); ++x)
                {
                    Farthest =
                        std::max(Farthest, t_Pyramid.Texel(Level, static_cast<uint32>(x), static_cast<uint32>(y)));
                }
            }
            return t_Bounds.NearDepth > Farthest + DepthBias;
        }
    } // namespace Occlusion
} // namespace Fling
