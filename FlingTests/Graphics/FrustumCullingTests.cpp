#include "Components/Transform.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingMath.h"
#include "Frustum.h"
#include "MeshCuller.h"
#include "MeshProxyTable.h"
#include "MeshRenderer.h"
#include "RenderSnapshot.h"
#include "SimRandom.h"
#include "Systems/PublishSnapshotSystem.h"
#include "Systems/TransformSystem.h"

#include <catch2/catch_all.hpp>
#include <chrono>
#include <limits>
#include <memory>
#include <vector>

using namespace Fling;

namespace
{
    /**
     * The renderer's matrices for a camera at t_Eye looking at t_Target: a 90 degree
     * perspective, aspect 1, near 0.1, far 100, with Vulkan's y flip when t_FlipY (the
     * offscreen pass flips it).
     */
    glm::mat4 ViewProjection(const glm::vec3& t_Eye, const glm::vec3& t_Target, const bool t_FlipY = true)
    {
        glm::mat4 Projection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 100.0f);
        if (t_FlipY)
        {
            Projection[1][1] *= -1.0f;
        }
        const glm::mat4 View = glm::lookAt(t_Eye, t_Target, glm::vec3(0.0f, 1.0f, 0.0f));
        return Projection * View;
    }

    /** The default camera of these tests: at the origin, looking down -z. */
    glm::mat4 ForwardCamera(const bool t_FlipY = true)
    {
        return ViewProjection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, -1.0f), t_FlipY);
    }

    /** What the GPU keeps: the point's clip coordinates are in -w <= x, y <= w and 0 <= z <= w. */
    bool InClipVolume(const glm::mat4& t_ViewProjection, const glm::vec3& t_Point)
    {
        const glm::vec4 Clip = t_ViewProjection * glm::vec4(t_Point, 1.0f);
        return Clip.w > 0.0f && -Clip.w <= Clip.x && Clip.x <= Clip.w && -Clip.w <= Clip.y && Clip.y <= Clip.w &&
               0.0f <= Clip.z && Clip.z <= Clip.w;
    }

    bool InsideAllPlanes(const Frustum& t_Frustum, const glm::vec3& t_Point)
    {
        for (const glm::vec4& Plane : t_Frustum.Planes)
        {
            if (glm::dot(glm::vec3(Plane), t_Point) + Plane.w < 0.0f)
            {
                return false;
            }
        }
        return true;
    }

    const Aabb UnitCube{glm::vec3(-0.5f), glm::vec3(0.5f)};

    glm::mat4 At(const glm::vec3& t_Position)
    {
        return glm::translate(glm::mat4(1.0f), t_Position);
    }

    float Uniform(SimRandom& t_Random, const float t_Min, const float t_Max)
    {
        return t_Random.Range(t_Min, t_Max);
    }

    /** A world with only Engine.Transform and Graphics.PublishSnapshot, as the renderer sees it. */
    struct PublishHarness
    {
        EntityStorage Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        PublishHarness()
        {
            Systems.Add<TransformSystem>();
            Systems.Add<PublishSnapshotSystem>();
            ScheduleResult schedule = Systems.BuildSchedule();
            REQUIRE(schedule.ErrorReport() == "");
            SystemRunnerConfig config;
            config.OneTickPerFrame = true;
            Runner = std::make_unique<SystemRunner>(Systems, std::move(schedule.Value), Storage, config);
            Runner->Create();
        }

        ~PublishHarness() { Runner->Destroy(); }

        const RenderSnapshot& Frame()
        {
            Runner->RunTick(WorldRole::Play);
            Runner->RunFrame(std::chrono::milliseconds(16), WorldRole::Play);
            return Storage.Singleton<RenderSnapshot>();
        }

        Entity Mesh(const glm::vec3& t_Position)
        {
            const Entity Made = Storage.Create();
            Storage.Add<Transform>(Made, Transform{t_Position});
            Storage.Add<MeshRenderer>(Made, MeshRenderer("Models/cube.obj"));
            return Made;
        }
    };

    /** The renderer's per frame steps on the CPU side: sync the proxies, give new slots their bounds, cull. */
    struct PassHarness
    {
        MeshProxyTable Proxies;
        MeshCuller Culler;
        std::vector<uint32> ItemSlots;

        bool Draw(const RenderSnapshot& t_Snapshot, const Frustum* t_Frustum)
        {
            MeshProxyTable::SyncResult Result = Proxies.Sync(t_Snapshot.Meshes);
            for (const uint32 Slot : Result.Removed)
            {
                Culler.ClearSlot(Slot);
            }
            for (const uint32 Slot : Result.Added)
            {
                Culler.SetSlot(Slot, true, UnitCube);
            }
            ItemSlots = std::move(Result.ItemSlots);
            return Culler.Cull(t_Snapshot.Meshes, ItemSlots, t_Frustum);
        }

        bool Visible(const Entity t_Entity) const { return Culler.IsVisible(Proxies.SlotOf(t_Entity.ToBits())); }
    };
} // namespace

TEST_CASE("Frustum: the planes keep exactly the points of Vulkan's clip volume", "[graphics][culling]")
{
    for (const bool bFlipY : {true, false})
    {
        CAPTURE(bFlipY);
        const glm::mat4 Matrix = ForwardCamera(bFlipY);
        const Frustum View = Frustum::FromViewProjection(Matrix);

        // Each plane is normalized, and the near and far planes face along the view axis
        for (const glm::vec4& Plane : View.Planes)
        {
            REQUIRE(glm::length(glm::vec3(Plane)) == Catch::Approx(1.0f));
        }
        REQUIRE(View.Planes[Frustum::Near].z == Catch::Approx(-1.0f));
        REQUIRE(View.Planes[Frustum::Near].w == Catch::Approx(-0.1f));
        REQUIRE(View.Planes[Frustum::Far].z == Catch::Approx(1.0f));
        REQUIRE(View.Planes[Frustum::Far].w == Catch::Approx(100.0f));

        REQUIRE(InsideAllPlanes(View, glm::vec3(0.0f, 0.0f, -1.0f)));
        // Outside each of the six planes in turn
        REQUIRE_FALSE(InsideAllPlanes(View, glm::vec3(-2.0f, 0.0f, -1.0f)));
        REQUIRE_FALSE(InsideAllPlanes(View, glm::vec3(2.0f, 0.0f, -1.0f)));
        REQUIRE_FALSE(InsideAllPlanes(View, glm::vec3(0.0f, -2.0f, -1.0f)));
        REQUIRE_FALSE(InsideAllPlanes(View, glm::vec3(0.0f, 2.0f, -1.0f)));
        REQUIRE_FALSE(InsideAllPlanes(View, glm::vec3(0.0f, 0.0f, -0.05f)));
        REQUIRE_FALSE(InsideAllPlanes(View, glm::vec3(0.0f, 0.0f, -101.0f)));

        // The same answer as the clip test for points all around the camera
        SimRandom Random(7, 1);
        int Inside = 0;
        for (int i = 0; i < 2000; ++i)
        {
            const glm::vec3 Point(
                Uniform(Random, -60.0f, 60.0f), Uniform(Random, -60.0f, 60.0f), Uniform(Random, -120.0f, 10.0f));
            const glm::vec4 Clip = Matrix * glm::vec4(Point, 1.0f);
            // Points within a hair of a plane can round either way
            const float Margin = 1e-3f * glm::abs(Clip.w);
            if (glm::abs(Clip.w - glm::abs(Clip.x)) < Margin || glm::abs(Clip.w - glm::abs(Clip.y)) < Margin ||
                glm::abs(Clip.z) < Margin || glm::abs(Clip.w - Clip.z) < Margin)
            {
                continue;
            }
            CAPTURE(Point.x, Point.y, Point.z);
            const bool bInClip = InClipVolume(Matrix, Point);
            REQUIRE(InsideAllPlanes(View, Point) == bInClip);
            Inside += bInClip ? 1 : 0;
        }
        // Both answers were tested many times
        REQUIRE(Inside > 100);
        REQUIRE(Inside < 1900);
    }
}

TEST_CASE("Frustum: boxes inside, outside each plane, straddling, and behind the camera", "[graphics][culling]")
{
    const Frustum View = Frustum::FromViewProjection(ForwardCamera());

    // At z = -5 the view is 5 wide on each side of the axis
    REQUIRE(View.Intersects(UnitCube, At({0.0f, 0.0f, -5.0f})));
    REQUIRE_FALSE(View.Intersects(UnitCube, At({-10.0f, 0.0f, -5.0f})));
    REQUIRE_FALSE(View.Intersects(UnitCube, At({10.0f, 0.0f, -5.0f})));
    REQUIRE_FALSE(View.Intersects(UnitCube, At({0.0f, -10.0f, -5.0f})));
    REQUIRE_FALSE(View.Intersects(UnitCube, At({0.0f, 10.0f, -5.0f})));
    REQUIRE_FALSE(View.Intersects(UnitCube, At({0.0f, 0.0f, -200.0f})));

    // Behind the camera, also where the clip w is negative
    REQUIRE_FALSE(View.Intersects(UnitCube, At({0.0f, 0.0f, 5.0f})));
    REQUIRE_FALSE(View.Intersects(UnitCube, At({3.0f, 3.0f, 5.0f})));

    // The centre is outside the right plane, but the near left edge is inside
    REQUIRE(View.Intersects(UnitCube, At({5.3f, 0.0f, -5.0f})));
    // The centre is past the far plane, but the near face is inside
    REQUIRE(View.Intersects(UnitCube, At({0.0f, 0.0f, -100.3f})));
    // Across the near plane, around the camera
    REQUIRE(View.Intersects(UnitCube, At({0.0f, 0.0f, 0.0f})));
    // A box much bigger than the view, around the camera
    REQUIRE(View.Intersects(Aabb{glm::vec3(-1000.0f), glm::vec3(1000.0f)}, glm::mat4(1.0f)));

    // A box whose bounds are not known is never culled
    REQUIRE_FALSE(Aabb{}.IsValid());
    REQUIRE(View.Intersects(Aabb{}, At({0.0f, 0.0f, 50.0f})));
}

TEST_CASE("Frustum: the world matrix places, turns and scales the box", "[graphics][culling]")
{
    const Frustum View = Frustum::FromViewProjection(ForwardCamera());

    // A long thin box whose centre is outside the right plane at z = -5 (x = 7)
    const Aabb Rod{glm::vec3(-4.0f, -0.1f, -0.1f), glm::vec3(4.0f, 0.1f, 0.1f)};
    REQUIRE(View.Intersects(Rod, At({7.0f, 0.0f, -5.0f})));
    // Turned to point along y, it no longer reaches across the right plane
    const glm::mat4 Turned = glm::rotate(At({7.0f, 0.0f, -5.0f}), glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
    REQUIRE_FALSE(View.Intersects(Rod, Turned));

    // A unit cube at x = 8 is outside; scaled by 6 it reaches back to z = -8, where the view is 8 wide
    REQUIRE_FALSE(View.Intersects(UnitCube, At({8.0f, 0.0f, -5.0f})));
    REQUIRE(View.Intersects(UnitCube, glm::scale(At({8.0f, 0.0f, -5.0f}), glm::vec3(6.0f))));
}

TEST_CASE("Frustum: a turned camera culls by its own view", "[graphics][culling]")
{
    // At (0, 0, 10), looking along +x
    const glm::vec3 Eye(0.0f, 0.0f, 10.0f);
    const Frustum View = Frustum::FromViewProjection(ViewProjection(Eye, Eye + glm::vec3(1.0f, 0.0f, 0.0f)));
    REQUIRE(View.Intersects(UnitCube, At(Eye + glm::vec3(5.0f, 0.0f, 0.0f))));
    REQUIRE_FALSE(View.Intersects(UnitCube, At(Eye + glm::vec3(-5.0f, 0.0f, 0.0f))));
    REQUIRE_FALSE(View.Intersects(UnitCube, At(Eye + glm::vec3(0.0f, 0.0f, -5.0f))));
    REQUIRE_FALSE(View.Intersects(UnitCube, At(glm::vec3(0.0f))));

    // Looking down, a little turned: what is under the camera is in view, what is above is not
    const Frustum Down = Frustum::FromViewProjection(ViewProjection(Eye, Eye + glm::vec3(0.1f, -1.0f, 0.2f)));
    REQUIRE(Down.Intersects(UnitCube, At(Eye + glm::vec3(0.0f, -5.0f, 0.0f))));
    REQUIRE_FALSE(Down.Intersects(UnitCube, At(Eye + glm::vec3(0.0f, 5.0f, 0.0f))));
}

TEST_CASE("Frustum: a box with any point in the clip volume is never culled", "[graphics][culling]")
{
    // Random cameras and random turned, scaled boxes. A culled box must have no visible
    // point: test its corners, its centre and a grid through it against the clip volume.
    SimRandom Random(11, 1);
    int Culled = 0;
    int Kept = 0;
    for (int i = 0; i < 400; ++i)
    {
        const glm::vec3 Eye(Uniform(Random, -5.0f, 5.0f), Uniform(Random, -5.0f, 5.0f), Uniform(Random, -5.0f, 5.0f));
        const glm::vec3 Look(Uniform(Random, -1.0f, 1.0f), Uniform(Random, -0.9f, 0.9f), Uniform(Random, -1.0f, 1.0f));
        if (glm::length(Look) < 0.1f)
        {
            continue;
        }
        const glm::mat4 Matrix = ViewProjection(Eye, Eye + Look);
        const Frustum View = Frustum::FromViewProjection(Matrix);

        const glm::vec3 Half(Uniform(Random, 0.1f, 3.0f), Uniform(Random, 0.1f, 3.0f), Uniform(Random, 0.1f, 3.0f));
        const Aabb Box{-Half, Half};
        glm::mat4 World = At(
            glm::vec3(Uniform(Random, -30.0f, 30.0f), Uniform(Random, -30.0f, 30.0f), Uniform(Random, -30.0f, 30.0f)));
        World =
            glm::rotate(World,
                        Uniform(Random, 0.0f, 6.28f),
                        glm::normalize(glm::vec3(
                            Uniform(Random, 0.1f, 1.0f), Uniform(Random, -1.0f, 1.0f), Uniform(Random, -1.0f, 1.0f))));
        World = glm::scale(World, glm::vec3(Uniform(Random, 0.5f, 2.0f)));

        if (View.Intersects(Box, World))
        {
            ++Kept;
            continue;
        }
        ++Culled;
        constexpr int Steps = 6;
        for (int x = 0; x <= Steps; ++x)
        {
            for (int y = 0; y <= Steps; ++y)
            {
                for (int z = 0; z <= Steps; ++z)
                {
                    const glm::vec3 Local = Box.Min + (Box.Max - Box.Min) * glm::vec3(x, y, z) / float(Steps);
                    const glm::vec3 Point = glm::vec3(World * glm::vec4(Local, 1.0f));
                    CAPTURE(i, Point.x, Point.y, Point.z);
                    REQUIRE_FALSE(InClipVolume(Matrix, Point));
                }
            }
        }
    }
    REQUIRE(Culled > 50);
    REQUIRE(Kept > 20);
}

TEST_CASE("Frustum culling: the pass draws a straddling mesh and skips an off-screen one", "[graphics][culling]")
{
    PublishHarness World;
    const Entity InView = World.Mesh({0.0f, 0.0f, -5.0f});
    const Entity Behind = World.Mesh({0.0f, 0.0f, 5.0f});
    const Entity FarRight = World.Mesh({20.0f, 0.0f, -5.0f});
    // The centre is right of the view at z = -5 (x = 5); the left half is inside
    const Entity Straddling = World.Mesh({5.3f, 0.0f, -5.0f});

    const Frustum View = Frustum::FromViewProjection(ForwardCamera());
    PassHarness Pass;
    const RenderSnapshot& Snapshot = World.Frame();
    // Culling is the renderer's: the snapshot still has every mesh, so no proxy is freed
    REQUIRE(Snapshot.Meshes.size() == 4);
    REQUIRE(Pass.Draw(Snapshot, &View));
    REQUIRE(Pass.Proxies.LiveCount() == 4);

    REQUIRE(Pass.Visible(InView));
    REQUIRE(Pass.Visible(Straddling));
    REQUIRE_FALSE(Pass.Visible(Behind));
    REQUIRE_FALSE(Pass.Visible(FarRight));
    REQUIRE(Pass.Culler.Stats().Drawn == 2);
    REQUIRE(Pass.Culler.Stats().Culled == 2);

    // The same frame again changes nothing, so the recorded draw list stays
    REQUIRE_FALSE(Pass.Draw(World.Frame(), &View));

    // The mesh moves into view: a change, and it is drawn
    World.Storage.Get<Transform>(FarRight).Position = glm::vec3(1.0f, 0.0f, -5.0f);
    REQUIRE(Pass.Draw(World.Frame(), &View));
    REQUIRE(Pass.Visible(FarRight));
    REQUIRE(Pass.Culler.Stats().Drawn == 3);

    // The camera turns around: what was behind it is in view, the rest is not
    const Frustum Back = Frustum::FromViewProjection(ViewProjection(glm::vec3(0.0f), glm::vec3(0.0f, 0.0f, 1.0f)));
    REQUIRE(Pass.Draw(World.Frame(), &Back));
    REQUIRE(Pass.Visible(Behind));
    REQUIRE_FALSE(Pass.Visible(InView));
    REQUIRE_FALSE(Pass.Visible(Straddling));
    REQUIRE_FALSE(Pass.Visible(FarRight));

    // Culling off: every mesh is drawn
    REQUIRE(Pass.Draw(World.Frame(), nullptr));
    REQUIRE(Pass.Culler.Stats().Drawn == 4);
    REQUIRE(Pass.Culler.Stats().Culled == 0);

    // A destroyed mesh's slot is cleared, and a new mesh in it starts from its own test
    World.Storage.Destroy(InView);
    REQUIRE_FALSE(Pass.Draw(World.Frame(), nullptr));
    REQUIRE(Pass.Culler.Stats().Drawn == 3);
}

TEST_CASE("Frustum culling: a slot the pass does not draw is neither drawn nor culled", "[graphics][culling]")
{
    MeshCuller Culler;
    std::vector<MeshDrawItem> Items(2);
    Items[0].World = At({0.0f, 0.0f, -5.0f});
    Items[1].World = At({0.0f, 0.0f, -5.0f});
    const std::vector<uint32> Slots = {0, 1};
    Culler.SetSlot(0, true, UnitCube);
    // Another pass draws slot 1 (a debug material)
    Culler.SetSlot(1, false, UnitCube);

    const Frustum View = Frustum::FromViewProjection(ForwardCamera());
    REQUIRE(Culler.Cull(Items, Slots, &View));
    REQUIRE(Culler.IsVisible(0));
    REQUIRE_FALSE(Culler.IsVisible(1));
    REQUIRE(Culler.Stats().Drawn == 1);
    REQUIRE(Culler.Stats().Culled == 0);
    // A slot it never heard of is not visible
    REQUIRE_FALSE(Culler.IsVisible(7));
}

TEST_CASE("Frustum: a box grows point by point, and a NaN point never makes it cullable", "[graphics][culling]")
{
    Aabb Box;
    REQUIRE_FALSE(Box.IsValid());
    Box.Add({1.0f, 2.0f, 3.0f});
    REQUIRE(Box.IsValid());
    REQUIRE(Box.Min == glm::vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(Box.Max == glm::vec3(1.0f, 2.0f, 3.0f));
    Box.Add({-1.0f, 5.0f, 0.0f});
    REQUIRE(Box.Min == glm::vec3(-1.0f, 2.0f, 0.0f));
    REQUIRE(Box.Max == glm::vec3(1.0f, 5.0f, 3.0f));

    // Of is the same as adding each point
    const std::vector<glm::vec3> Points = {{1.0f, 2.0f, 3.0f}, {-1.0f, 5.0f, 0.0f}};
    const Aabb Made = Aabb::Of(Points);
    REQUIRE(Made.Min == Box.Min);
    REQUIRE(Made.Max == Box.Max);
    REQUIRE_FALSE(Aabb::Of({}).IsValid());

    // A NaN point never lets the box lose a point in view, wherever it comes
    const float NaN = std::numeric_limits<float>::quiet_NaN();
    const Frustum View = Frustum::FromViewProjection(ForwardCamera());
    const glm::vec3 InView{0.0f, 0.0f, -5.0f};
    const glm::vec3 FarAway{1000.0f, 0.0f, -5.0f};
    for (const std::vector<glm::vec3>& Order : std::vector<std::vector<glm::vec3>>{
             {glm::vec3(NaN), InView, FarAway}, {InView, glm::vec3(NaN), FarAway}, {InView, FarAway, glm::vec3(NaN)}})
    {
        Aabb Mixed;
        for (const glm::vec3& Point : Order)
        {
            Mixed.Add(Point);
        }
        REQUIRE(View.Intersects(Mixed, glm::mat4(1.0f)));
    }
}
