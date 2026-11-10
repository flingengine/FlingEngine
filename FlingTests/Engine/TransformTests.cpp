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
#include <vector>

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

TEST_CASE("Engine.Transform handles deep and wide trees, and a child that is new", "[transform][hierarchy]")
{
    QuietLogger();
    TransformHarness h;
    // A chain 300 deep: each link adds 1 on x
    std::vector<Entity> chain{h.Make(glm::vec3(1.0f, 0.0f, 0.0f))};
    for (int i = 1; i < 300; ++i)
    {
        chain.push_back(h.Make(glm::vec3(1.0f, 0.0f, 0.0f)));
        REQUIRE(HierarchyUtil::SetParent(h.Storage, chain.back(), chain[chain.size() - 2]));
    }
    // A root with 500 children
    const Entity hub = h.Make(glm::vec3(0.0f, 5.0f, 0.0f));
    std::vector<Entity> spokes;
    for (int i = 0; i < 500; ++i)
    {
        spokes.push_back(h.Make(glm::vec3(float(i), 0.0f, 0.0f)));
        REQUIRE(HierarchyUtil::SetParent(h.Storage, spokes.back(), hub));
    }

    // The first tick: no entity has a LocalToWorld yet, and every one gets the right matrix
    h.Runner->RunTick(WorldRole::Play);
    for (size_t i = 0; i < chain.size(); ++i)
    {
        REQUIRE(h.WorldPos(chain[i]) == glm::vec3(float(i + 1), 0.0f, 0.0f));
    }
    for (size_t i = 0; i < spokes.size(); ++i)
    {
        REQUIRE(h.WorldPos(spokes[i]) == glm::vec3(float(i), 5.0f, 0.0f));
    }
    REQUIRE_FALSE(h.Storage.Singleton<TransformWork>().WarnedOutOfStep);

    // A move at the top reaches the bottom in the same tick
    h.Storage.Get<Transform>(chain[0]).Position.y = 2.0f;
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.WorldPos(chain.back()) == glm::vec3(300.0f, 2.0f, 0.0f));

    // A child that is new under a parent that has its matrix
    const Entity late = h.Make(glm::vec3(0.0f, 0.0f, 3.0f));
    REQUIRE(HierarchyUtil::SetParent(h.Storage, late, spokes[7]));
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.WorldPos(late) == glm::vec3(7.0f, 5.0f, 3.0f));
    REQUIRE(h.Storage.Singleton<TransformWork>().Pending.empty());
}

TEST_CASE("Engine.Transform treats a child with no usable parent as a root", "[transform][hierarchy]")
{
    QuietLogger();
    TransformHarness h;
    // The parent has no Transform
    const Entity plain = h.Storage.Create();
    const Entity child = h.Make(glm::vec3(1.0f, 2.0f, 3.0f));
    const Entity grandchild = h.Make(glm::vec3(1.0f, 0.0f, 0.0f));
    REQUIRE(HierarchyUtil::SetParent(h.Storage, child, plain));
    REQUIRE(HierarchyUtil::SetParent(h.Storage, grandchild, child));
    // The parent is dead, and the link was not cleaned up
    const Entity gone = h.Make(glm::vec3(100.0f, 0.0f, 0.0f));
    const Entity orphan = h.Make(glm::vec3(0.0f, 4.0f, 0.0f));
    h.Storage.Add<Parent>(orphan, Parent{gone});
    h.Storage.Destroy(gone);

    h.Runner->RunTick(WorldRole::Play);
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.WorldPos(child) == glm::vec3(1.0f, 2.0f, 3.0f));
    REQUIRE(h.WorldPos(grandchild) == glm::vec3(2.0f, 2.0f, 3.0f));
    REQUIRE(h.WorldPos(orphan) == glm::vec3(0.0f, 4.0f, 0.0f));
    REQUIRE_FALSE(h.Storage.Singleton<TransformWork>().WarnedOutOfStep);
}

TEST_CASE("Engine.Transform follows Parent when Children does not agree with it", "[transform][hierarchy]")
{
    QuietLogger();
    TransformHarness h;
    const Entity root = h.Make(glm::vec3(10.0f, 0.0f, 0.0f));
    const Entity child = h.Make(glm::vec3(0.0f, 1.0f, 0.0f));
    const Entity grandchild = h.Make(glm::vec3(0.0f, 0.0f, 2.0f));
    // Parent set by hand, so no entity has Children
    h.Storage.Add<Parent>(child, Parent{root});
    h.Storage.Add<Parent>(grandchild, Parent{child});

    h.Runner->RunTick(WorldRole::Play);
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.WorldPos(child) == glm::vec3(10.0f, 1.0f, 0.0f));
    REQUIRE(h.WorldPos(grandchild) == glm::vec3(10.0f, 1.0f, 2.0f));
    REQUIRE(h.Storage.Singleton<TransformWork>().WarnedOutOfStep);

    // A Children list that names an entity with another parent does not move that entity
    const Entity other = h.Make(glm::vec3(-5.0f, 0.0f, 0.0f));
    h.Storage.Add<Children>(other).Value.push_back(grandchild);
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.WorldPos(grandchild) == glm::vec3(10.0f, 1.0f, 2.0f));
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

TEST_CASE("RebuildChildren clears old Children and breaks a loop of Parent links", "[hierarchy]")
{
    QuietLogger();
    EntityStorage storage;
    const Entity a = storage.Create();
    const Entity b = storage.Create();
    const Entity stale = storage.Create();

    // A Children list with no Parent link behind it goes away
    storage.Add<Children>(stale, Children{{a}});

    // a -> b -> a has no root
    storage.Add<Parent>(a, Parent{b});
    storage.Add<Parent>(b, Parent{a});
    HierarchyUtil::RebuildChildren(storage);

    REQUIRE_FALSE(storage.Has<Children>(stale));
    REQUIRE(storage.Has<Parent>(a) != storage.Has<Parent>(b)); // One link is removed
    const Entity root = storage.Has<Parent>(a) ? b : a;
    const Entity leaf = root == a ? b : a;
    REQUIRE(HierarchyUtil::GetParent(storage, leaf) == root);
    REQUIRE(storage.Get<Children>(root).Value == std::vector<Entity>{leaf});
    REQUIRE_FALSE(storage.Has<Children>(leaf));
}

TEST_CASE("SetParent refuses dead entities, and the same parent is no change", "[hierarchy]")
{
    QuietLogger();
    EntityStorage storage;
    const Entity parent = storage.Create();
    const Entity child = storage.Create();
    const Entity dead = storage.Create();
    storage.Destroy(dead);

    REQUIRE_FALSE(HierarchyUtil::SetParent(storage, dead, parent));
    REQUIRE_FALSE(HierarchyUtil::SetParent(storage, child, dead));
    REQUIRE_FALSE(storage.Has<Parent>(child));

    REQUIRE(HierarchyUtil::SetParent(storage, child, parent));
    REQUIRE(HierarchyUtil::SetParent(storage, child, parent));
    REQUIRE(storage.Get<Children>(parent).Value == std::vector<Entity>{child});

    // A null parent detaches, and the parent's empty Children goes away
    REQUIRE(HierarchyUtil::SetParent(storage, child, Entity::Null));
    REQUIRE_FALSE(storage.Has<Parent>(child));
    REQUIRE_FALSE(storage.Has<Children>(parent));

    // Destroying a dead root does nothing
    HierarchyUtil::DestroySubtree(storage, dead);
    REQUIRE(storage.IsValid(parent));
}
