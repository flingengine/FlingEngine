#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Ecs/TypeRegistry.h"
#include "HierarchyUtil.h"
#include "Json.h"
#include "Logger.h"
#include "Systems/TransformSystem.h"
#include "TransformUtil.h"

#include <catch2/catch_all.hpp>
#include <memory>

using namespace Fling;

namespace
{
    void QuietLogger()
    {
        Logger::Get().Init();
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    bool NearlyEqual(const glm::vec3& a, const glm::vec3& b, const float margin = 0.001f)
    {
        return glm::all(glm::lessThan(glm::abs(a - b), glm::vec3(margin)));
    }

    /** A storage with only Engine.Transform in its schedule. */
    struct TransformHarness
    {
        EntityStorage Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        TransformHarness()
        {
            Systems.Add<TransformSystem>();
            ScheduleResult schedule = Systems.BuildSchedule();
            REQUIRE(schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(schedule.Value), Storage);
            Runner->Create();
        }

        ~TransformHarness() { Runner->Destroy(); }

        Entity Make(const glm::vec3& position)
        {
            const Entity e = Storage.Create();
            Storage.Add<Transform>(e, Transform{position});
            return e;
        }

        glm::vec3 WorldPos(const Entity e) { return TransformUtil::WorldPosition(Storage.Get<LocalToWorld>(e)); }
    };
} // namespace

TEST_CASE("TransformUtil makes a translate, rotate, scale matrix", "[transform]")
{
    Transform t;
    t.Position = glm::vec3(1.0f, 2.0f, 3.0f);
    t.Rotation = TransformUtil::FromEulerDegrees(glm::vec3(0.0f, 90.0f, 0.0f));
    t.Scale = glm::vec3(2.0f);

    const glm::mat4 m = TransformUtil::LocalMatrix(t);
    // A point one unit along +X: scaled to 2, turned 90 degrees around Y to -Z, then moved
    const glm::vec3 p = glm::vec3(m * glm::vec4(1.0f, 0.0f, 0.0f, 1.0f));
    REQUIRE(NearlyEqual(p, glm::vec3(1.0f, 2.0f, 1.0f)));
    REQUIRE(TransformUtil::WorldPosition(LocalToWorld{m}) == t.Position);
}

TEST_CASE("Euler degrees round trip through a quaternion", "[transform]")
{
    const glm::vec3 degrees(10.0f, 30.0f, 20.0f);
    REQUIRE(NearlyEqual(TransformUtil::ToEulerDegrees(TransformUtil::FromEulerDegrees(degrees)), degrees));

    Transform t;
    TransformUtil::RotateAround(t, glm::vec3(0.0f, 1.0f, 0.0f), 45.0f);
    TransformUtil::RotateAround(t, glm::vec3(0.0f, 1.0f, 0.0f), 45.0f);
    REQUIRE(NearlyEqual(TransformUtil::ToEulerDegrees(t.Rotation), glm::vec3(0.0f, 90.0f, 0.0f)));
    REQUIRE(NearlyEqual(TransformUtil::Forward(t), glm::vec3(-1.0f, 0.0f, 0.0f)));
}

TEST_CASE("A version 1 Transform with Euler angles loads as a quaternion", "[transform][reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterEngine(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);
    const TypeDesc& type = *types.Find<Transform>();
    REQUIRE(type.Version == 2);

    // The shape of every level file from before this version
    const Json old = Json::Parse(R"({ "position": [4, 5, 6], "rotation": [0, 90, 0], "scale": [1, 2, 1] })");
    Transform loaded;
    LoadObject(type, &loaded, old, {});
    REQUIRE(loaded.Position == glm::vec3(4.0f, 5.0f, 6.0f));
    REQUIRE(loaded.Scale == glm::vec3(1.0f, 2.0f, 1.0f));
    REQUIRE(NearlyEqual(TransformUtil::ToEulerDegrees(loaded.Rotation), glm::vec3(0.0f, 90.0f, 0.0f)));

    // It saves as version 2, and loads back the same
    Json saved = Json::Object();
    SaveObject(type, &loaded, saved, {});
    REQUIRE(saved.GetInt64("$v") == 2);
    Transform again;
    LoadObject(type, &again, saved, {});
    REQUIRE(again.Rotation == loaded.Rotation);
}

TEST_CASE("Engine.Transform computes LocalToWorld, parents first", "[transform][hierarchy]")
{
    QuietLogger();
    TransformHarness h;
    const Entity root = h.Make(glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity child = h.Make(glm::vec3(0.0f, 1.0f, 0.0f));
    const Entity grandchild = h.Make(glm::vec3(0.0f, 0.0f, 2.0f));
    // Linked after they were made, so the order in storage is not the depth order
    REQUIRE(HierarchyUtil::SetParent(h.Storage, grandchild, child));
    REQUIRE(HierarchyUtil::SetParent(h.Storage, child, root));

    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Has<LocalToWorld>(root));
    REQUIRE(h.WorldPos(root) == glm::vec3(10.0f, 0.0f, 0.0f));
    REQUIRE(h.WorldPos(child) == glm::vec3(10.0f, 1.0f, 0.0f));
    REQUIRE(h.WorldPos(grandchild) == glm::vec3(10.0f, 1.0f, 2.0f));

    // The version goes up only when the matrix changes
    const uint32 rootVersion = h.Storage.Get<LocalToWorld>(root).Version;
    const uint32 childVersion = h.Storage.Get<LocalToWorld>(child).Version;
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Get<LocalToWorld>(root).Version == rootVersion);

    // Moving the root moves the subtree
    h.Storage.Get<Transform>(root).Position.x = 20.0f;
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Get<LocalToWorld>(root).Version == rootVersion + 1);
    REQUIRE(h.Storage.Get<LocalToWorld>(child).Version == childVersion + 1);
    REQUIRE(h.WorldPos(grandchild) == glm::vec3(20.0f, 1.0f, 2.0f));

    // A rotated parent turns the child's offset
    h.Storage.Get<Transform>(child).Rotation = TransformUtil::FromEulerDegrees(glm::vec3(0.0f, 90.0f, 0.0f));
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(NearlyEqual(h.WorldPos(grandchild), glm::vec3(22.0f, 1.0f, 0.0f)));

    // It runs in the editor's world too
    h.Storage.Get<Transform>(root).Position.x = 0.0f;
    h.Runner->RunTick(WorldRole::Edit);
    REQUIRE(h.WorldPos(root) == glm::vec3(0.0f));
}

TEST_CASE("HierarchyUtil keeps Parent and Children in step", "[hierarchy]")
{
    QuietLogger();
    EntityStorage storage;
    const Entity a = storage.Create();
    const Entity b = storage.Create();
    const Entity c = storage.Create();

    REQUIRE(HierarchyUtil::SetParent(storage, b, a));
    REQUIRE(HierarchyUtil::SetParent(storage, c, a));
    REQUIRE(storage.Get<Children>(a).Value == std::vector<Entity>{b, c});
    REQUIRE(HierarchyUtil::GetParent(storage, b) == a);

    // A loop is refused
    REQUIRE_FALSE(HierarchyUtil::SetParent(storage, a, c));
    REQUIRE_FALSE(storage.Has<Parent>(a));

    // Moving c under b updates both lists
    REQUIRE(HierarchyUtil::SetParent(storage, c, b));
    REQUIRE(storage.Get<Children>(a).Value == std::vector<Entity>{b});
    REQUIRE(storage.Get<Children>(b).Value == std::vector<Entity>{c});
    REQUIRE(HierarchyUtil::IsAncestorOf(storage, a, c));

    // Detach removes the empty list
    HierarchyUtil::Detach(storage, c);
    REQUIRE_FALSE(storage.Has<Children>(b));
    REQUIRE_FALSE(storage.Has<Parent>(c));
}

TEST_CASE("Destroying a parent destroys its subtree", "[hierarchy]")
{
    QuietLogger();
    EntityStorage storage;
    const Entity keep = storage.Create();
    const Entity root = storage.Create();
    const Entity child = storage.Create();
    const Entity grandchild = storage.Create();
    HierarchyUtil::SetParent(storage, root, keep);
    HierarchyUtil::SetParent(storage, child, root);
    HierarchyUtil::SetParent(storage, grandchild, child);

    HierarchyUtil::DestroySubtree(storage, root);
    REQUIRE(storage.IsValid(keep));
    REQUIRE_FALSE(storage.IsValid(root));
    REQUIRE_FALSE(storage.IsValid(child));
    REQUIRE_FALSE(storage.IsValid(grandchild));
    REQUIRE_FALSE(storage.Has<Children>(keep));
}

TEST_CASE("RebuildChildren makes Children from Parent and drops bad links", "[hierarchy]")
{
    QuietLogger();
    EntityStorage storage;
    const Entity parent = storage.Create();
    const Entity child = storage.Create();
    const Entity orphan = storage.Create();
    const Entity dead = storage.Create();
    storage.Destroy(dead);

    storage.Add<Parent>(child, Parent{parent});
    storage.Add<Parent>(orphan, Parent{dead});
    HierarchyUtil::RebuildChildren(storage);

    REQUIRE(storage.Get<Children>(parent).Value == std::vector<Entity>{child});
    REQUIRE_FALSE(storage.Has<Parent>(orphan));
}
