#include "../Common/CountingNew.h"
#include "Components/Transform.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"
#include "HierarchyUtil.h"
#include "Systems/TransformSystem.h"

#include <array>
#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>

// Proves that a steady simulation tick takes no memory from the global heap: chunks,
// sparse slots, and singletons come from the world's BlockAllocator, and deferred
// values and callables from each command buffer's arena.

using namespace Fling;

namespace
{
    struct Mover
    {
        static constexpr ComponentInfo Component{.Name = "Alloc.Mover"};
        float X = 0.0f;
        float Speed = 1.0f;
    };

    /** A table component that comes and goes, so its entity moves archetype. */
    struct Boost
    {
        static constexpr ComponentInfo Component{.Name = "Alloc.Boost"};
        float Amount = 0.0f;
    };

    /** A sparse component that comes and goes. */
    struct Marked
    {
        static constexpr ComponentInfo Component{.Name = "Alloc.Marked", .Storage = ComponentStorage::Sparse};
        int Count = 0;
    };

    /** Aligned past the default new alignment, to test the aligned column paths. */
    struct alignas(64) Wide
    {
        static constexpr ComponentInfo Component{.Name = "Alloc.Wide"};
        float Values[16] = {};
    };

    constexpr size_t RingSize = 64;

    /** The entities the churn system made, oldest first. A fixed array, so it never allocates. */
    struct Spawned
    {
        static constexpr ComponentInfo Component{.Name = "Alloc.Spawned",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Keep};
        std::array<Entity, RingSize> Ring{};
        uint32 Next = 0;
        uint32 Ticks = 0;
        uint32 Deferred = 0;
    };

    /** Reads and writes with both query styles. */
    class MoveSystem final : public System<MoveSystem, Access<Write<Mover>, Read<Boost>, Write<Wide>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Alloc.Move", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, mover] : ctx.Query<Mover>(Without<Boost>{}))
            {
                mover.X += mover.Speed;
            }
            ctx.Query<Mover, const Boost>().Each([](Mover& mover, const Boost& boost) { mover.X += boost.Amount; });
            ctx.Query<Wide, const Mover>().Each([](Wide& wide, const Mover& mover) { wide.Values[0] = mover.X; });
        }
    };

    /** Adds and removes components, spawns, destroys, and defers, every tick. */
    class ChurnSystem final
        : public System<
              ChurnSystem,
              Access<Write<Spawned>, Write<Mover>, Write<Boost>, Write<Marked>, Write<Wide>, Read<SimTime>, Spawns>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Alloc.Churn", .In = Order::ResolveDeferred};

        void OnCreate(Setup& setup) const { setup.AddSingleton<Spawned>(); }

        void OnDestroy(Setup& setup) const { setup.RemoveSingleton<Spawned>(); }

        void FixedUpdate(Context& ctx) const
        {
            Spawned& spawned = ctx.Singleton<Spawned>();
            const uint64 tick = ctx.Singleton<const SimTime>().Tick;

            // Swap Boost and Marked on and off, a different half of the entities each tick.
            for (auto [entity, mover] : ctx.Query<const Mover>())
            {
                if (((entity.ToBits() + tick) & 1) == 0)
                {
                    ctx.Add(entity, Boost{0.5f});
                    ctx.Add(entity, Marked{1});
                }
                else
                {
                    ctx.Remove<Boost>(entity);
                    ctx.Remove<Marked>(entity);
                }
            }

            // Destroy the oldest entity and spawn a new one in its place in the ring.
            Entity& slot = spawned.Ring[spawned.Next];
            if (slot != Entity{})
            {
                ctx.Destroy(slot);
            }
            slot = ctx.Spawn(Mover{0.0f, 2.0f}, Wide{});
            spawned.Next = (spawned.Next + 1) % RingSize;
            spawned.Ticks++;

            Spawned* target = &spawned;
            ctx.Defer<Spawned>([target](EntityStorage&) { target->Deferred++; });
        }
    };
} // namespace

TEST_CASE("A steady tick takes no memory from the global heap", "[ecs][memory]")
{
    if (!FLING_TEST_COUNT_ALLOCATIONS)
    {
        SKIP("This build has no counting operator new (ThreadSanitizer)");
    }
    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<MoveSystem>();
    systems.Add<ChurnSystem>();
    ScheduleResult schedule = systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    SystemRunner runner(systems, std::move(schedule.Value), storage, SystemRunnerConfig{});
    runner.Create();

    for (int i = 0; i < 256; ++i)
    {
        const Entity e = storage.Create();
        storage.Add<Mover>(e, Mover{float(i), 1.0f});
    }

    // Warm up: the archetypes, their edges, the chunks, the sparse pages, and the scratch lists all reach their size.
    for (int i = 0; i < 2 * int(RingSize) + 8; ++i)
    {
        runner.RunTick(WorldRole::Play);
    }

    g_Allocations = 0;
    g_CountAllocations = true;
    constexpr int MeasuredTicks = 200;
    for (int i = 0; i < MeasuredTicks; ++i)
    {
        runner.RunTick(WorldRole::Play);
    }
    g_CountAllocations = false;

    CHECK(g_Allocations.load() == 0);
    const Spawned& spawned = *storage.TryGetSingleton<Spawned>();
    CHECK(spawned.Deferred == spawned.Ticks);
    CHECK(spawned.Ticks == 2 * RingSize + 8 + MeasuredTicks);

    // The world's memory is in its own blocks, and TrimMemory gives back what is empty.
    const EcsStats before = storage.Native().Stats();
    CHECK(before.ReservedBytes > 0);
    CHECK(before.UsedBytes <= before.ReservedBytes);
    runner.Destroy();

    // With every entity gone, Trim gives most of the pages back.
    storage.Clear();
    CHECK(storage.TrimMemory() > 0);
    const EcsStats after = storage.Stats();
    CHECK(after.Entities == 0);
    CHECK(after.ReservedBytes < before.ReservedBytes);
}

TEST_CASE("Engine.Transform takes no memory from the global heap in a steady tick", "[ecs][memory][transform]")
{
    if (!FLING_TEST_COUNT_ALLOCATIONS)
    {
        SKIP("This build has no counting operator new (ThreadSanitizer)");
    }
    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<TransformSystem>();
    ScheduleResult schedule = systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    SystemRunner runner(systems, std::move(schedule.Value), storage, SystemRunnerConfig{});
    runner.Create();

    // Roots, and chains four deep below some of them
    std::vector<Entity> movers;
    for (int i = 0; i < 200; ++i)
    {
        Entity above = storage.Create(Transform{glm::vec3(float(i), 0.0f, 0.0f)});
        movers.push_back(above);
        for (int depth = 0; depth < (i % 4 == 0 ? 4 : 0); ++depth)
        {
            const Entity child = storage.Create(Transform{glm::vec3(0.0f, 1.0f, 0.0f)});
            REQUIRE(HierarchyUtil::SetParent(storage, child, above));
            above = child;
        }
    }

    // Warm up: LocalToWorld is added, and the work list reaches its size
    for (int i = 0; i < 4; ++i)
    {
        runner.RunTick(WorldRole::Play);
    }

    g_Allocations = 0;
    g_CountAllocations = true;
    for (int i = 0; i < 50; ++i)
    {
        for (const Entity mover : movers)
        {
            storage.Get<Transform>(mover).Position.z += 1.0f;
        }
        runner.RunTick(WorldRole::Play);
    }
    g_CountAllocations = false;

    CHECK(g_Allocations.load() == 0);
    runner.Destroy();
}
