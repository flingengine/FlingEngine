#include "FlingMath.h"
#include "OcclusionTest.h"
#include "SimRandom.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <limits>
#include <vector>

using namespace Fling;

namespace
{
    constexpr uint32 Size = 64;

    glm::mat4 ViewProjection(const glm::vec3& t_Eye, const glm::vec3& t_Target)
    {
        glm::mat4 Projection = glm::perspective(glm::radians(80.0f), 1.0f, 0.1f, 200.0f);
        Projection[1][1] *= -1.0f;
        return Projection * glm::lookAt(t_Eye, t_Target, glm::vec3(0.0f, 1.0f, 0.0f));
    }

    struct Box
    {
        Aabb Local;
        glm::mat4 World{1.0f};
    };

    /** A pixel ray: the world positions at depth 0 and depth 1 of the pixel centre */
    struct Ray
    {
        glm::vec3 Origin;
        glm::vec3 Dir;
    };

    Ray PixelRay(const glm::mat4& t_InverseVp, const uint32 t_X, const uint32 t_Y)
    {
        const float Nx = (static_cast<float>(t_X) + 0.5f) / Size * 2.0f - 1.0f;
        const float Ny = (static_cast<float>(t_Y) + 0.5f) / Size * 2.0f - 1.0f;
        auto Unproject = [&](const float t_Z)
        {
            const glm::vec4 P = t_InverseVp * glm::vec4(Nx, Ny, t_Z, 1.0f);
            return glm::vec3(P) / P.w;
        };
        const glm::vec3 Near = Unproject(0.0f);
        return {Near, glm::normalize(Unproject(1.0f) - Near)};
    }

    /** The depth (0 to 1) where the ray first meets the box, or a negative number for a miss */
    float HitDepth(const Box& t_Box, const Ray& t_Ray, const glm::mat4& t_Vp)
    {
        const glm::mat4 Inverse = glm::inverse(t_Box.World);
        const glm::vec3 O = glm::vec3(Inverse * glm::vec4(t_Ray.Origin, 1.0f));
        const glm::vec3 D = glm::vec3(Inverse * glm::vec4(t_Ray.Dir, 0.0f));
        float T0 = 0.0f;
        float T1 = std::numeric_limits<float>::max();
        for (int Axis = 0; Axis < 3; ++Axis)
        {
            if (std::abs(D[Axis]) < 1.0e-9f)
            {
                if (O[Axis] < t_Box.Local.Min[Axis] || O[Axis] > t_Box.Local.Max[Axis])
                {
                    return -1.0f;
                }
                continue;
            }
            float A = (t_Box.Local.Min[Axis] - O[Axis]) / D[Axis];
            float B = (t_Box.Local.Max[Axis] - O[Axis]) / D[Axis];
            if (A > B)
            {
                std::swap(A, B);
            }
            T0 = std::max(T0, A);
            T1 = std::min(T1, B);
            if (T0 > T1)
            {
                return -1.0f;
            }
        }
        const glm::vec4 Clip = t_Vp * glm::vec4(t_Ray.Origin + t_Ray.Dir * T0, 1.0f);
        return Clip.z / Clip.w;
    }

    float Random(SimRandom& t_Rng, const float t_Lo, const float t_Hi)
    {
        return t_Lo + (t_Hi - t_Lo) * t_Rng.NextFloat01();
    }
} // namespace

TEST_CASE("DepthPyramid keeps the farthest depth under each texel", "[Occlusion]")
{
    SimRandom Rng(7, 1);
    for (const auto& [W, H] : std::vector<std::pair<uint32, uint32>>{{8, 8}, {5, 3}, {1, 7}, {13, 1}, {64, 33}, {1, 1}})
    {
        std::vector<float> Depth(size_t{W} * H);
        for (float& D : Depth)
        {
            D = Rng.NextFloat01();
        }
        DepthPyramid Pyramid;
        Pyramid.Build(Depth, W, H);
        REQUIRE(Pyramid.LevelCount() == DepthPyramid::LevelsFor(W, H));
        REQUIRE(Pyramid.Width(Pyramid.LevelCount() - 1) == 1);
        REQUIRE(Pyramid.Height(Pyramid.LevelCount() - 1) == 1);

        // The last level is the farthest depth of the whole image
        REQUIRE(Pyramid.Texel(Pyramid.LevelCount() - 1, 0, 0) == *std::max_element(Depth.begin(), Depth.end()));

        // Every texel of the image is covered by a texel of each level that is at least as far
        for (uint32 L = 1; L < Pyramid.LevelCount(); ++L)
        {
            for (uint32 y = 0; y < H; ++y)
            {
                for (uint32 x = 0; x < W; ++x)
                {
                    const uint32 Lx = std::min(x >> L, Pyramid.Width(L) - 1);
                    const uint32 Ly = std::min(y >> L, Pyramid.Height(L) - 1);
                    REQUIRE(Pyramid.Texel(L, Lx, Ly) >= Depth[size_t{y} * W + x]);
                }
            }
        }
    }
}

TEST_CASE("A wall hides a box behind it and not a box in front of it", "[Occlusion]")
{
    const glm::mat4 Vp = ViewProjection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f));
    const glm::mat4 InverseVp = glm::inverse(Vp);

    Box Wall;
    Wall.Local = Aabb::Of(std::vector<glm::vec3>{{-3, -3, -0.5f}, {3, 3, 0.5f}});
    Wall.World = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));

    std::vector<float> Depth(size_t{Size} * Size, 1.0f);
    for (uint32 y = 0; y < Size; ++y)
    {
        for (uint32 x = 0; x < Size; ++x)
        {
            const float D = HitDepth(Wall, PixelRay(InverseVp, x, y), Vp);
            if (D >= 0.0f)
            {
                Depth[size_t{y} * Size + x] = D;
            }
        }
    }
    DepthPyramid Pyramid;
    Pyramid.Build(Depth, Size, Size);

    const Aabb Unit = Aabb::Of(std::vector<glm::vec3>{{-0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, 0.5f}});
    const auto Test = [&](const glm::vec3& t_At)
    {
        return Occlusion::IsOccluded(Pyramid,
                                     Occlusion::ProjectBounds(Unit, glm::translate(glm::mat4(1.0f), t_At), Vp));
    };

    CHECK(Test(glm::vec3(0.0f, 0.0f, -20.0f)));
    CHECK(Test(glm::vec3(2.0f, 1.0f, -30.0f)));
    CHECK_FALSE(Test(glm::vec3(0.0f, 0.0f, -5.0f)));
    // Beside the wall, in the clear
    CHECK_FALSE(Test(glm::vec3(10.0f, 0.0f, -20.0f)));
    // Reaching the plane of the camera: never hidden
    CHECK_FALSE(Test(glm::vec3(0.0f, 0.0f, 0.0f)));
    CHECK_FALSE(Test(glm::vec3(0.0f, 0.0f, 3.0f)));
    // A box that is not known
    CHECK_FALSE(Occlusion::IsOccluded(Pyramid, Occlusion::ProjectBounds(Aabb{}, glm::mat4(1.0f), Vp)));
}

TEST_CASE("Occlusion never hides a mesh that can be seen", "[Occlusion]")
{
    SimRandom Rng(2024, 1);
    uint32 Hidden = 0;
    uint32 Tested = 0;

    for (uint32 Trial = 0; Trial < 150; ++Trial)
    {
        // A random scene of boxes, turned and scaled, and a random camera
        std::vector<Box> Boxes(24);
        for (Box& B : Boxes)
        {
            const glm::vec3 Half{Random(Rng, 0.2f, 4.0f), Random(Rng, 0.2f, 4.0f), Random(Rng, 0.2f, 1.5f)};
            B.Local = Aabb::Of(std::vector<glm::vec3>{-Half, Half});
            B.World =
                glm::translate(
                    glm::mat4(1.0f),
                    glm::vec3(Random(Rng, -25.0f, 25.0f), Random(Rng, -10.0f, 10.0f), Random(Rng, -60.0f, 5.0f))) *
                glm::rotate(glm::mat4(1.0f), Random(Rng, 0.0f, 6.28f), glm::vec3(0.0f, 1.0f, 0.0f));
        }
        const glm::vec3 Eye{Random(Rng, -8.0f, 8.0f), Random(Rng, -3.0f, 3.0f), Random(Rng, 0.0f, 12.0f)};
        const glm::vec3 Target{Random(Rng, -10.0f, 10.0f), Random(Rng, -5.0f, 5.0f), Random(Rng, -50.0f, -10.0f)};
        const glm::mat4 Vp = ViewProjection(Eye, Target);
        const glm::mat4 InverseVp = glm::inverse(Vp);

        // The depth image of every box, a box included: the least that a pyramid could hold
        std::vector<float> Depth(size_t{Size} * Size, 1.0f);
        std::vector<std::vector<float>> Own(Boxes.size(), std::vector<float>(size_t{Size} * Size, -1.0f));
        for (uint32 y = 0; y < Size; ++y)
        {
            for (uint32 x = 0; x < Size; ++x)
            {
                const Ray R = PixelRay(InverseVp, x, y);
                for (size_t i = 0; i < Boxes.size(); ++i)
                {
                    const float D = HitDepth(Boxes[i], R, Vp);
                    Own[i][size_t{y} * Size + x] = D;
                    if (D >= 0.0f)
                    {
                        Depth[size_t{y} * Size + x] = std::min(Depth[size_t{y} * Size + x], D);
                    }
                }
            }
        }
        DepthPyramid Pyramid;
        Pyramid.Build(Depth, Size, Size);

        for (size_t i = 0; i < Boxes.size(); ++i)
        {
            ++Tested;
            if (!Occlusion::IsOccluded(Pyramid, Occlusion::ProjectBounds(Boxes[i].Local, Boxes[i].World, Vp)))
            {
                continue;
            }
            ++Hidden;
            // Hidden: the box must not be the nearest thing at any pixel where it is hit
            for (size_t p = 0; p < Depth.size(); ++p)
            {
                if (Own[i][p] >= 0.0f)
                {
                    INFO("trial " << Trial << " box " << i << " pixel " << p);
                    REQUIRE(Own[i][p] > Depth[p]);
                }
            }
        }
    }
    // The test is not empty: some boxes were hidden and some were not
    CHECK(Hidden > 20);
    CHECK(Hidden < Tested);
}
