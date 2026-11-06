#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <entt/entity/registry.hpp>
#include <memory>
#include <string>
#include <vector>

// Fixture systems that exist only for these tests, so they are declared here
// (Plans/ecs-systems.md section 3.3). Real systems are a header plus a .cpp.

using namespace Fling;
using namespace std::chrono_literals;

namespace
{
    struct Position
    {
        static constexpr ComponentInfo Component{.Name = "Test.Position"};
        float X = 0.0f;
    };

    struct Velocity
    {
        static constexpr ComponentInfo Component{.Name = "Test.Velocity"};
        float X = 0.0f;
    };

    struct Frozen
    {
        static constexpr ComponentInfo Component{.Name = "Test.Frozen", .Kind = ComponentKind::Tag};
    };

    /** Points at another entity. */
    struct Follow
    {
        static constexpr ComponentInfo Component{.Name = "Test.Follow"};
        Entity Target;
        float Seen = -1.0f;
    };

    struct Bounds
    {
        static constexpr ComponentInfo Component{
            .Name = "Test.Bounds", .Kind = ComponentKind::Singleton, .Transient = true};
        float MaxX = 100.0f;
    };

    struct SpawnRequest
    {
        float X = 0.0f;
    };

    struct SpawnQueue
    {
        static constexpr ComponentInfo Component{
            .Name = "Test.SpawnQueue", .Kind = ComponentKind::Queue, .Transient = true};
        std::vector<SpawnRequest> Requests;
    };

    /** Smoothed for display. Written at the frame rate. */
    struct Display
    {
        static constexpr ComponentInfo Component{.Name = "Test.Display", .Domain = ComponentDomain::Presentation};
        float X = 0.0f;
        float LastDelta = 0.0f;
    };

    /** Counts calls, so the tests can see which hooks ran. */
    struct Calls
    {
        static constexpr ComponentInfo Component{
            .Name = "Test.Calls", .Kind = ComponentKind::Singleton, .Transient = true};
        int Created = 0;
        int PlayBegan = 0;
        int PlayEnded = 0;
        int Ticks = 0;
        int EditTicks = 0;
        uint64 LastTick = 0;
    };

    /** Owns Bounds and Calls, and counts ticks. */
    class SetupSystem final : public System<SetupSystem, Access<Write<Bounds>, Write<Calls>, Read<SimTime>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Setup", .Phase = Phase::Input};

        void OnCreate(Setup& setup) const
        {
            setup.AddSingleton<Bounds>(Bounds{10.0f});
            setup.AddSingleton<Calls>().Created++;
        }

        void OnDestroy(Setup& setup) const
        {
            setup.RemoveSingleton<Bounds>();
            setup.RemoveSingleton<Calls>();
        }

        void OnPlayBegin(Context& ctx) const { ctx.Singleton<Calls>().PlayBegan++; }

        void OnPlayEnd(Context& ctx) const { ctx.Singleton<Calls>().PlayEnded++; }

        void FixedUpdate(Context& ctx) const
        {
            Calls& calls = ctx.Singleton<Calls>();
            calls.Ticks++;
            calls.LastTick = ctx.Singleton<const SimTime>().Tick;
        }
    };

    /** Moves every entity that is not frozen. */
    class MoveSystem final
        : public System<MoveSystem, Access<Read<SimTime>, Read<Velocity>, Read<Frozen>, Write<Position>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Move", .Phase = Phase::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float dt = ctx.Singleton<const SimTime>().FixedDelta;
            for (auto [entity, position, velocity] : ctx.Query<Position, const Velocity>(Without<Frozen>{}))
            {
                position.X += velocity.X * dt;
            }
        }
    };

    /** Destroys entities that leave the bounds. Destruction waits until the end of the tick. */
    class CullSystem final : public System<CullSystem, Access<Read<Bounds>, Read<Position>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Cull", .Phase = Phase::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float maxX = ctx.Singleton<const Bounds>().MaxX;
            for (auto [entity, position] : ctx.Query<const Position>())
            {
                if (position.X > maxX)
                {
                    ctx.Commands().Destroy(entity);
                }
            }
        }
    };

    /** Asks for one spawn per frozen entity. */
    class RequestSpawnSystem final
        : public System<RequestSpawnSystem, Access<Read<Position>, Read<Frozen>, Append<SpawnQueue>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.RequestSpawn", .Phase = Phase::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, position] : ctx.Query<const Position>(With<Frozen>{}))
            {
                ctx.Queue<SpawnQueue>().Push(SpawnRequest{position.X + 1.0f});
            }
        }
    };

    /** The one place where spawns happen. */
    class ResolveSpawnSystem final
        : public System<ResolveSpawnSystem, Access<Consume<SpawnQueue>, Write<Position>, Spawns>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.ResolveSpawn", .Phase = Phase::ResolveDeferred};

        void OnCreate(Setup& setup) const { setup.AddSingleton<SpawnQueue>(); }

        void OnDestroy(Setup& setup) const { setup.RemoveSingleton<SpawnQueue>(); }

        void FixedUpdate(Context& ctx) const
        {
            for (const SpawnRequest& request : ctx.Drain<SpawnQueue>())
            {
                ctx.Spawn(Position{request.X});
            }
        }
    };

    /** Follows a handle to another entity. A stale handle gives null. */
    class FollowSystem final : public System<FollowSystem, Access<Read<Position>, Write<Follow>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Follow", .Phase = Phase::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, follow] : ctx.Query<Follow>())
            {
                const Position* target = ctx.TryGet<const Position>(follow.Target);
                follow.Seen = target != nullptr ? target->X : -1.0f;
            }
        }
    };

    /** Runs only in the edit world. */
    class EditOnlySystem final : public System<EditOnlySystem, Access<Write<Calls>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.EditOnly", .Phase = Phase::Simulation, .RunIn = WorldRole::Edit};

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Calls>().EditTicks++; }
    };

    /** A frame rate system. It reads the simulation and writes a Presentation component. */
    class DisplaySystem final
        : public System<DisplaySystem, Access<Read<RealFrameTime>, Read<Position>, Write<Display>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Display", .Phase = Phase::PostSimulation};

        void Update(Context& ctx) const
        {
            const float delta = ctx.Singleton<const RealFrameTime>().DeltaSeconds;
            for (auto [entity, display, position] : ctx.Query<Display, const Position>())
            {
                display.X = position.X;
                display.LastDelta = delta;
            }
        }
    };

    /** Makes a registry, a runner, and the systems, and calls Create. */
    struct Harness
    {
        entt::registry Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        void Start(SystemRunnerConfig config = {})
        {
            ScheduleResult schedule = Systems.BuildSchedule();
            REQUIRE(schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(schedule.Value), Storage, config);
            Runner->Create();
        }

        entt::entity Make(float x, float velocity)
        {
            const entt::entity e = Storage.create();
            Storage.assign<Position>(e, Position{x});
            Storage.assign<Velocity>(e, Velocity{velocity});
            return e;
        }
    };
} // namespace

TEST_CASE("The adapter finds the functions a system writes", "[ecs][system]")
{
    STATIC_REQUIRE(Detail::HasFixedUpdate<MoveSystem>);
    STATIC_REQUIRE_FALSE(Detail::HasUpdate<MoveSystem>);
    STATIC_REQUIRE_FALSE(Detail::HasOnCreate<MoveSystem>);
    STATIC_REQUIRE(Detail::HasOnCreate<SetupSystem>);
    STATIC_REQUIRE(Detail::HasOnPlayBegin<SetupSystem>);
    STATIC_REQUIRE(Detail::HasUpdate<DisplaySystem>);
    STATIC_REQUIRE_FALSE(Detail::HasFixedUpdate<DisplaySystem>);

    STATIC_REQUIRE(sizeof(MoveSystem) == sizeof(ISystem));
    STATIC_REQUIRE(Detail::CheckSystem<MoveSystem>());
}

TEST_CASE("Describe is made from Info and the access list", "[ecs][system]")
{
    const MoveSystem system;
    const SystemDesc& desc = system.Describe();
    REQUIRE(std::string(desc.Info.Name) == "Test.Move");
    REQUIRE(desc.Info.Phase == Phase::Simulation);
    REQUIRE(desc.Access.size() == 4);
    REQUIRE(desc.Access[3].Hash == TypeHashOf<Position>);
    REQUIRE(desc.Access[3].Mode == AccessMode::Write);
    REQUIRE_FALSE(desc.Spawns);
    REQUIRE(desc.PrepareStorage != nullptr);
    REQUIRE(ResolveSpawnSystem{}.Describe().Spawns);
}

TEST_CASE("Entity converts to and from EnTT", "[ecs][system]")
{
    entt::registry storage;
    const entt::entity first = storage.create();
    storage.destroy(first);
    const entt::entity reused = storage.create(); // Same index, next version.

    const Entity handle = Detail::FromEntt(reused);
    REQUIRE(handle.Index == Detail::FromEntt(first).Index);
    REQUIRE(handle.Generation == Detail::FromEntt(first).Generation + 1);
    REQUIRE(Detail::ToEntt(handle) == reused);
    REQUIRE(Detail::FromEntt(entt::null).IsNull());
    const bool nullMaps = Detail::ToEntt(Entity::Null) == entt::null;
    REQUIRE(nullMaps);
}

TEST_CASE("OnCreate and OnDestroy make and remove singletons", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start();

    REQUIRE(h.Storage.try_ctx<Bounds>() != nullptr);
    REQUIRE(h.Storage.ctx<Calls>().Created == 1);
    REQUIRE(h.Storage.try_ctx<SimTime>() != nullptr);

    h.Runner->Destroy();
    REQUIRE(h.Storage.try_ctx<Bounds>() == nullptr);
    REQUIRE(h.Storage.try_ctx<Calls>() == nullptr);
    REQUIRE(h.Storage.try_ctx<SimTime>() == nullptr);
}

TEST_CASE("A fixed system queries with filters and SimTime", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<MoveSystem>();
    h.Start();

    const entt::entity moving = h.Make(0.0f, 60.0f);
    const entt::entity frozen = h.Make(0.0f, 60.0f);
    h.Storage.assign<Frozen>(frozen);

    h.Runner->RunTick(WorldRole::Play);

    REQUIRE(h.Storage.get<Position>(moving).X == Catch::Approx(1.0f));
    REQUIRE(h.Storage.get<Position>(frozen).X == 0.0f);
    REQUIRE(h.Storage.ctx<SimTime>().Tick == 1);
}

TEST_CASE("Destroy waits until the end of the tick", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Systems.Add<CullSystem>();
    h.Systems.Add<FollowSystem>();
    h.Start();

    const entt::entity gone = h.Make(50.0f, 0.0f);
    const entt::entity kept = h.Make(5.0f, 0.0f);
    const entt::entity follower = h.Storage.create();
    h.Storage.assign<Follow>(follower, Follow{Detail::FromEntt(gone)});

    // Cull and Follow share a stage, so in this tick Follow still sees the entity.
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE_FALSE(h.Storage.valid(gone));
    REQUIRE(h.Storage.valid(kept));
    REQUIRE(h.Storage.get<Follow>(follower).Seen == 50.0f);

    // Next tick the handle is stale. TryGet gives null and never reads a reused slot.
    h.Storage.create();
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.get<Follow>(follower).Seen == -1.0f);
}

TEST_CASE("Appenders queue, and one system spawns at ResolveDeferred", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<RequestSpawnSystem>();
    h.Systems.Add<ResolveSpawnSystem>();
    h.Start();

    const entt::entity source = h.Make(3.0f, 0.0f);
    h.Storage.assign<Frozen>(source);

    h.Runner->RunTick(WorldRole::Play);

    std::vector<float> xs;
    for (const entt::entity e : h.Storage.view<Position>())
    {
        xs.push_back(h.Storage.get<Position>(e).X);
    }
    std::sort(xs.begin(), xs.end());
    REQUIRE(xs == std::vector<float>{3.0f, 4.0f});
    REQUIRE(h.Storage.ctx<SpawnQueue>().Requests.empty());
}

TEST_CASE("RunIn skips systems for other world roles", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Systems.Add<EditOnlySystem>();
    h.Start();

    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.ctx<Calls>().EditTicks == 0);

    h.Runner->RunTick(WorldRole::Edit);
    REQUIRE(h.Storage.ctx<Calls>().EditTicks == 1);
    REQUIRE(h.Storage.ctx<Calls>().Ticks == 1); // SetupSystem runs in Play only.
}

TEST_CASE("PlayBegin and PlayEnd call the hooks", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start();

    h.Runner->PlayBegin(WorldRole::Play);
    h.Runner->PlayEnd(WorldRole::Play);
    REQUIRE(h.Storage.ctx<Calls>().PlayBegan == 1);
    REQUIRE(h.Storage.ctx<Calls>().PlayEnded == 1);
}

TEST_CASE("The accumulator runs one tick per fixed step", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start(SystemRunnerConfig{.FixedStep = 10ms});

    REQUIRE(h.Runner->RunFixed(5ms, WorldRole::Play) == 0);
    REQUIRE(h.Runner->RunFixed(5ms, WorldRole::Play) == 1);
    REQUIRE(h.Runner->RunFixed(20ms, WorldRole::Play) == 2);
    REQUIRE(h.Storage.ctx<Calls>().LastTick == 3);

    // A long stall runs at most MaxTicksPerFrame, and drops the rest.
    REQUIRE(h.Runner->RunFixed(10s, WorldRole::Play) == SystemRunnerConfig{}.MaxTicksPerFrame);
    REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 0);
}

TEST_CASE("OneTickPerFrame ignores real time", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start(SystemRunnerConfig{.OneTickPerFrame = true});

    REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 1);
    REQUIRE(h.Runner->RunFixed(1s, WorldRole::Play) == 1);
    REQUIRE(h.Runner->TickCount() == 2);
}

TEST_CASE("A frame system reads RealFrameTime and writes Presentation", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<MoveSystem>();
    h.Systems.Add<DisplaySystem>();
    h.Start();

    const entt::entity e = h.Make(0.0f, 60.0f);
    h.Storage.assign<Display>(e);

    h.Runner->RunFixed(SystemRunnerConfig{}.FixedStep, WorldRole::Play);
    h.Runner->RunFrame(20ms, WorldRole::Play);

    REQUIRE(h.Storage.get<Display>(e).X == Catch::Approx(1.0f));
    REQUIRE(h.Storage.get<Display>(e).LastDelta == Catch::Approx(0.02f));
    REQUIRE(h.Storage.ctx<RealFrameTime>().Elapsed == Catch::Approx(0.02));
}
