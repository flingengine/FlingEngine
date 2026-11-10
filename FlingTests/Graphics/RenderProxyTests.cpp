#include "Components/Transform.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "HierarchyUtil.h"
#include "Lighting/PointLight.hpp"
#include "MeshProxyTable.h"
#include "MeshRenderer.h"
#include "RenderSnapshot.h"
#include "Systems/PublishSnapshotSystem.h"
#include "Systems/TransformSystem.h"

#include <catch2/catch_all.hpp>
#include <chrono>
#include <memory>

using namespace Fling;

namespace
{
    MeshDrawItem Item(const uint64 key, const char* mesh, const char* material = "")
    {
        MeshDrawItem item;
        item.Key = key;
        item.Mesh = mesh;
        item.Material = material;
        return item;
    }

    /** A world with only Engine.Transform and Graphics.PublishSnapshot. */
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

        /** One fixed tick (Engine.Transform) and the variable rate systems (Graphics.PublishSnapshot). */
        const RenderSnapshot& Frame()
        {
            Runner->RunTick(WorldRole::Play);
            Runner->RunFrame(std::chrono::milliseconds(16), WorldRole::Play);
            return Storage.Singleton<RenderSnapshot>();
        }
    };
} // namespace

TEST_CASE("MeshProxyTable adds, changes, and removes proxies", "[graphics][proxy]")
{
    MeshProxyTable table;

    const std::vector<MeshDrawItem> first = {Item(1, "a.obj"), Item(2, "b.obj")};
    MeshProxyTable::SyncResult result = table.Sync(first);
    REQUIRE(result.Added.size() == 2);
    REQUIRE(result.Removed.empty());
    REQUIRE(table.LiveCount() == 2);
    REQUIRE(result.ItemSlots.size() == 2);
    REQUIRE(result.ItemSlots[0] == table.SlotOf(1));
    REQUIRE(result.DrawListChanged());

    // The same snapshot again changes nothing
    result = table.Sync(first);
    REQUIRE_FALSE(result.DrawListChanged());

    // A new material path is a change, not a new proxy
    const std::vector<MeshDrawItem> changed = {Item(1, "a.obj", "m.mat"), Item(2, "b.obj")};
    result = table.Sync(changed);
    REQUIRE(result.Changed == std::vector<uint32>{table.SlotOf(1)});
    REQUIRE(table.At(table.SlotOf(1)).Material == "m.mat");

    // Mesh 1 is gone and mesh 3 is new: 3 takes the freed slot
    const uint32 freedSlot = table.SlotOf(1);
    const std::vector<MeshDrawItem> swapped = {Item(2, "b.obj"), Item(3, "c.obj")};
    result = table.Sync(swapped);
    REQUIRE(result.Removed == std::vector<uint32>{freedSlot});
    REQUIRE(result.Added == std::vector<uint32>{freedSlot});
    REQUIRE(table.SlotOf(1) == MeshProxyTable::InvalidSlot);
    REQUIRE(table.SlotOf(3) == freedSlot);
    REQUIRE(table.Capacity() == 2);

    // An empty snapshot frees everything
    result = table.Sync({});
    REQUIRE(result.Removed.size() == 2);
    REQUIRE(table.LiveCount() == 0);
}

TEST_CASE("Destroying an entity with a MeshRenderer releases its proxy slot", "[graphics][proxy]")
{
    PublishHarness h;
    const Entity kept = h.Storage.Create();
    h.Storage.Add<Transform>(kept, Transform{glm::vec3(1.0f, 2.0f, 3.0f)});
    h.Storage.Add<MeshRenderer>(kept, MeshRenderer("Models/cube.obj"));
    const Entity doomed = h.Storage.Create();
    h.Storage.Add<Transform>(doomed);
    h.Storage.Add<MeshRenderer>(doomed, MeshRenderer("Models/sphere.obj", "Materials/White.mat"));

    MeshProxyTable proxies;
    const RenderSnapshot& snapshot = h.Frame();
    REQUIRE(snapshot.Meshes.size() == 2);
    proxies.Sync(snapshot.Meshes);
    REQUIRE(proxies.LiveCount() == 2);
    const uint32 doomedSlot = proxies.SlotOf(doomed.ToBits());
    REQUIRE(doomedSlot != MeshProxyTable::InvalidSlot);

    h.Storage.Destroy(doomed);
    const MeshProxyTable::SyncResult result = proxies.Sync(h.Frame().Meshes);
    REQUIRE(result.Removed == std::vector<uint32>{doomedSlot});
    REQUIRE(proxies.LiveCount() == 1);
    REQUIRE(proxies.SlotOf(doomed.ToBits()) == MeshProxyTable::InvalidSlot);
    REQUIRE(proxies.SlotOf(kept.ToBits()) != MeshProxyTable::InvalidSlot);
}

TEST_CASE("An entity made after the last tick is drawn in its world place in the same frame", "[graphics][publish]")
{
    PublishHarness h;
    const Entity parent = h.Storage.Create(Transform{glm::vec3(10.0f, 0.0f, 0.0f)});
    h.Frame();

    // Made between the tick and the frame, as the editor and a level load do: no LocalToWorld yet
    const Entity child = h.Storage.Create(Transform{glm::vec3(0.0f, 1.0f, 0.0f)}, MeshRenderer("Models/cube.obj"));
    REQUIRE(HierarchyUtil::SetParent(h.Storage, child, parent));
    h.Runner->RunFrame(std::chrono::milliseconds(16), WorldRole::Play);
    REQUIRE_FALSE(h.Storage.Has<LocalToWorld>(child));

    const RenderSnapshot& snapshot = h.Storage.Singleton<RenderSnapshot>();
    REQUIRE(snapshot.Meshes.size() == 1);
    // Not at (0, 1, 0), which is where it is relative to its parent
    REQUIRE(glm::vec3(snapshot.Meshes[0].World[3]) == glm::vec3(10.0f, 1.0f, 0.0f));

    // The next tick gives it a LocalToWorld with the same place
    REQUIRE(glm::vec3(h.Frame().Meshes[0].World[3]) == glm::vec3(10.0f, 1.0f, 0.0f));
    REQUIRE(h.Storage.Has<LocalToWorld>(child));
}

TEST_CASE("Graphics.PublishSnapshot copies world matrices and lights into the snapshot", "[graphics][publish]")
{
    PublishHarness h;
    const Entity parent = h.Storage.Create();
    h.Storage.Add<Transform>(parent, Transform{glm::vec3(10.0f, 0.0f, 0.0f)});
    const Entity mesh = h.Storage.Create();
    h.Storage.Add<Transform>(mesh, Transform{glm::vec3(0.0f, 1.0f, 0.0f)});
    h.Storage.Add<Parent>(mesh, Parent{parent});
    h.Storage.Add<MeshRenderer>(mesh, MeshRenderer("Models/cube.obj"));
    const Entity light = h.Storage.Create();
    h.Storage.Add<Transform>(light, Transform{glm::vec3(0.0f, 5.0f, 0.0f)});
    h.Storage.Add<PointLight>(light);

    const RenderSnapshot& snapshot = h.Frame();
    const uint64 frame = snapshot.Frame;

    // The child is drawn at its world position, from LocalToWorld
    const MeshDrawItem* item = nullptr;
    for (const MeshDrawItem& each : snapshot.Meshes)
    {
        item = each.Key == mesh.ToBits() ? &each : item;
    }
    REQUIRE(item != nullptr);
    REQUIRE(item->Mesh == "Models/cube.obj");
    REQUIRE(glm::vec3(item->World[3]) == glm::vec3(10.0f, 1.0f, 0.0f));
    REQUIRE(item->WorldVersion != 0);

    REQUIRE(snapshot.PointLights.size() == 1);
    REQUIRE(h.Frame().Frame == frame + 1);
}
