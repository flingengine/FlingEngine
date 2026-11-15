#include "Ecs/EntityStorage.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <chrono>
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
        static constexpr ComponentInfo Component{.Name = "Test.Bounds",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Keep};
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
            .Name = "Test.Calls", .Kind = ComponentKind::Singleton, .Transient = true, .Rollback = RollbackClass::Keep};
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
        static constexpr SystemInfo Info{.Name = "Test.Setup", .In = Order::Input};

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
        static constexpr SystemInfo Info{.Name = "Test.Move", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float dt = ctx.Singleton<const SimTime>().DeltaSeconds();
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
        static constexpr SystemInfo Info{.Name = "Test.Cull", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float maxX = ctx.Singleton<const Bounds>().MaxX;
            for (auto [entity, position] : ctx.Query<const Position>())
            {
                if (position.X > maxX)
                {
                    ctx.Destroy(entity);
                }
            }
        }
    };

    /** Asks for one spawn per frozen entity. */
    class RequestSpawnSystem final
        : public System<RequestSpawnSystem, Access<Read<Position>, Read<Frozen>, Append<SpawnQueue>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.RequestSpawn", .In = Order::Simulation};

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
        static constexpr SystemInfo Info{.Name = "Test.ResolveSpawn", .In = Order::ResolveDeferred};

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
        static constexpr SystemInfo Info{.Name = "Test.Follow", .In = Order::Simulation};

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
        static constexpr SystemInfo Info{.Name = "Test.EditOnly", .In = Order::Simulation, .RunIn = WorldRole::Edit};

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Calls>().EditTicks++; }
    };

    /** A variable rate system. It reads the simulation and writes a Presentation component. */
    class DisplaySystem final
        : public System<DisplaySystem, Access<Read<RealFrameTime>, Read<Position>, Write<Display>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Display", .In = Order::Presentation};

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
        Fling::EntityStorage Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        void Start(SystemRunnerConfig config = {})
        {
            ScheduleResult schedule = Systems.BuildSchedule();
            REQUIRE(schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(schedule.Value), Storage, config);
            Runner->Create();
        }

        Fling::Entity Make(float x, float velocity)
        {
            const Fling::Entity e = Storage.Create();
            Storage.Add<Position>(e, Position{x});
            Storage.Add<Velocity>(e, Velocity{velocity});
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
    REQUIRE(String(desc.Info.Name) == "Test.Move");
    REQUIRE(&desc.Info.In.Get() == &Order::Simulation);
    REQUIRE(desc.Access.size() == 4);
    REQUIRE(desc.Access[3].Hash == TypeHashOf<Position>);
    REQUIRE(desc.Access[3].Mode == AccessMode::Write);
    REQUIRE_FALSE(desc.Spawns);
    REQUIRE(desc.Prepare != nullptr);
    REQUIRE(ResolveSpawnSystem{}.Describe().Spawns);
}

TEST_CASE("A destroyed entity's handle goes stale when its slot is used again", "[ecs][system]")
{
    Fling::EntityStorage storage;
    const Entity first = storage.Create();
    storage.Destroy(first);
    const Entity reused = storage.Create(); // Same index, next generation

    REQUIRE(reused.Index == first.Index);
    REQUIRE(reused.Generation == first.Generation + 1);
    REQUIRE(storage.IsValid(reused));
    REQUIRE_FALSE(storage.IsValid(first));
    REQUIRE_FALSE(storage.IsValid(Entity::Null));
    // A stale handle is ignored
    storage.Destroy(first);
    REQUIRE(storage.IsValid(reused));
}

TEST_CASE("Each component has its own type id and storage", "[ecs][system]")
{
    REQUIRE(Detail::ComponentIdOf<Position>() != Detail::ComponentIdOf<Velocity>());
    REQUIRE(Detail::ComponentIdOf<Velocity>() != Detail::ComponentIdOf<Frozen>());
    // The id comes from the saved name, so asking again gives the same one
    REQUIRE(Detail::ComponentIdOf<Position>() == Detail::ComponentIdOf<const Position>());
    REQUIRE(Detail::GetComponentOps(Detail::ComponentIdOf<Position>()).Key == TypeHashOf<Position>);

    Fling::EntityStorage storage;
    const Entity e = storage.Create();
    storage.Add<Position>(e, Position{1.0f});
    storage.Add<Velocity>(e, Velocity{2.0f});
    REQUIRE(storage.Get<Position>(e).X == 1.0f);
    REQUIRE(storage.Get<Velocity>(e).X == 2.0f);
}

TEST_CASE("OnCreate and OnDestroy make and remove singletons", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start();

    REQUIRE(h.Storage.TryGetSingleton<Bounds>() != nullptr);
    REQUIRE(h.Storage.Singleton<Calls>().Created == 1);
    REQUIRE(h.Storage.TryGetSingleton<SimTime>() != nullptr);

    h.Runner->Destroy();
    REQUIRE(h.Storage.TryGetSingleton<Bounds>() == nullptr);
    REQUIRE(h.Storage.TryGetSingleton<Calls>() == nullptr);
    REQUIRE(h.Storage.TryGetSingleton<SimTime>() == nullptr);
}

TEST_CASE("A fixed system queries with filters and SimTime", "[ecs][system]")
{
    // SimTime's delta is the tick length at the runner's rate: 60 units a second moves 2 a
    // tick at 30 Hz and 1 at 60 Hz
    for (const uint32 Hz : {30u, 60u})
    {
        CAPTURE(Hz);
        Harness h;
        h.Systems.Add<MoveSystem>();
        h.Start(SystemRunnerConfig{.SimHz = Hz});

        const Fling::Entity moving = h.Make(0.0f, 60.0f);
        const Fling::Entity frozen = h.Make(0.0f, 60.0f);
        h.Storage.Add<Frozen>(frozen);

        h.Runner->RunTick(WorldRole::Play);

        REQUIRE(h.Storage.Get<Position>(moving).X == Catch::Approx(60.0f / static_cast<float>(Hz)));
        REQUIRE(h.Storage.Get<Position>(frozen).X == 0.0f);
        REQUIRE(h.Storage.Singleton<SimTime>().Tick == 1);
        REQUIRE(h.Storage.Singleton<SimTime>().FixedDeltaNs == SimRate::StepNs(Hz));
    }
}

TEST_CASE("Destroy waits until the end of the tick", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Systems.Add<CullSystem>();
    h.Systems.Add<FollowSystem>();
    h.Start();

    const Fling::Entity gone = h.Make(50.0f, 0.0f);
    const Fling::Entity kept = h.Make(5.0f, 0.0f);
    const Fling::Entity follower = h.Storage.Create();
    h.Storage.Add<Follow>(follower, Follow{gone});

    // Cull and Follow share a stage, so in this tick Follow still sees the entity.
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE_FALSE(h.Storage.IsValid(gone));
    REQUIRE(h.Storage.IsValid(kept));
    REQUIRE(h.Storage.Get<Follow>(follower).Seen == 50.0f);

    // Next tick the handle is stale. TryGet gives null and never reads a reused slot.
    h.Storage.Create();
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Get<Follow>(follower).Seen == -1.0f);
}

TEST_CASE("Appenders queue, and one system spawns at ResolveDeferred", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<RequestSpawnSystem>();
    h.Systems.Add<ResolveSpawnSystem>();
    h.Start();

    const Fling::Entity source = h.Make(3.0f, 0.0f);
    h.Storage.Add<Frozen>(source);

    h.Runner->RunTick(WorldRole::Play);

    std::vector<float> xs;
    for (const Fling::Entity e : h.Storage.View<Position>())
    {
        xs.push_back(h.Storage.Get<Position>(e).X);
    }
    std::sort(xs.begin(), xs.end());
    REQUIRE(xs == std::vector<float>{3.0f, 4.0f});
    REQUIRE(h.Storage.Singleton<SpawnQueue>().Requests.empty());
}

TEST_CASE("RunIn skips systems for other world roles", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Systems.Add<EditOnlySystem>();
    h.Start();

    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Singleton<Calls>().EditTicks == 0);

    h.Runner->RunTick(WorldRole::Edit);
    REQUIRE(h.Storage.Singleton<Calls>().EditTicks == 1);
    REQUIRE(h.Storage.Singleton<Calls>().Ticks == 1); // SetupSystem runs in Play only.
}

TEST_CASE("PlayBegin and PlayEnd call the hooks", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start();

    h.Runner->PlayBegin(WorldRole::Play);
    h.Runner->PlayEnd(WorldRole::Play);
    REQUIRE(h.Storage.Singleton<Calls>().PlayBegan == 1);
    REQUIRE(h.Storage.Singleton<Calls>().PlayEnded == 1);
}

TEST_CASE("The accumulator runs one tick per fixed step", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start(SystemRunnerConfig{.SimHz = 100});

    REQUIRE(h.Runner->RunFixed(5ms, WorldRole::Play) == 0);
    REQUIRE(h.Runner->RunFixed(5ms, WorldRole::Play) == 1);
    REQUIRE(h.Runner->RunFixed(20ms, WorldRole::Play) == 2);
    REQUIRE(h.Storage.Singleton<Calls>().LastTick == 3);

    // A long stall runs at most MaxTicksPerWake, and drops the rest.
    REQUIRE(h.Runner->RunFixed(10s, WorldRole::Play) == SystemRunnerConfig{.SimHz = 100}.MaxTicksPerWake());
    REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 0);
}

TEST_CASE("The catch-up limit is a time, the same at every rate", "[ecs][system]")
{
    struct Rate
    {
        uint32 Hz;
        uint32 MaxTicks;
    };
    // MaxCatchUp (250 ms) in ticks, rounded up
    for (const Rate Each : {Rate{1, 1}, Rate{30, 8}, Rate{1000, 250}})
    {
        CAPTURE(Each.Hz);
        const SystemRunnerConfig Config{.SimHz = Each.Hz};
        REQUIRE(Config.MaxTicksPerWake() == Each.MaxTicks);

        // 60 fps frames for two seconds run every tick that is due: none is dropped
        Harness h;
        h.Systems.Add<SetupSystem>();
        h.Start(Config);
        const std::chrono::nanoseconds Frame{16'666'667};
        for (int f = 0; f < 120; ++f)
        {
            h.Runner->RunFixed(Frame, WorldRole::Play);
        }
        REQUIRE(h.Runner->TickCount() == 2 * Each.Hz);
        for (uint64 Tick = 1; Tick <= h.Runner->TickCount(); ++Tick)
        {
            const TickRecord* Record = h.Runner->GetLedger().Find(Tick);
            if (Record != nullptr)
            {
                REQUIRE(Record->DroppedSteps == 0);
            }
        }

        // A stall of ten seconds runs the limit, then drops the rest
        REQUIRE(h.Runner->RunFixed(10s, WorldRole::Play) == Each.MaxTicks);
        REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 0);
    }
}

TEST_CASE("OneTickPerFrame ignores real time", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<SetupSystem>();
    h.Start(SystemRunnerConfig{.OneTickPerFrame = true});

    REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 1);
    REQUIRE(h.Runner->RunFixed(1s, WorldRole::Play) == 1);
    REQUIRE(h.Runner->TickCount() == 2);

    // But not a pause
    constexpr int64 NowNs = 1'000'000'000;
    h.Runner->SetPaused(true, NowNs);
    REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 0);
    h.Runner->SetPaused(false, NowNs);
    REQUIRE(h.Runner->RunFixed(0ns, WorldRole::Play) == 1);
}

TEST_CASE("A variable rate system reads RealFrameTime and writes Presentation", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<MoveSystem>();
    h.Systems.Add<DisplaySystem>();
    h.Start();

    const Fling::Entity e = h.Make(0.0f, 60.0f);
    h.Storage.Add<Display>(e);

    h.Runner->RunFixed(SystemRunnerConfig{}.FixedStep(), WorldRole::Play);
    h.Runner->RunFrame(20ms, WorldRole::Play);

    // One tick at the default rate moved it 60 units a second for one tick length
    REQUIRE(h.Storage.Get<Display>(e).X == Catch::Approx(60.0f / static_cast<float>(SimRate::DefaultHz)));
    REQUIRE(h.Storage.Get<Display>(e).LastDelta == Catch::Approx(0.02f));
    REQUIRE(h.Storage.Singleton<RealFrameTime>().Elapsed == Catch::Approx(0.02));
}

namespace
{
    /** A module group with no systems yet, after Simulation. */
    inline constexpr GroupInfo LateGroup{.Name = "Test.Late", .In = Order::Tick, .After = Order::Simulation};

    class FirstWriterSystem final : public System<FirstWriterSystem, Access<Write<Position>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.FirstWriter", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, position] : ctx.Query<Position>())
            {
                position.X = 1.0f;
            }
        }
    };

    class SecondWriterSystem final : public System<SecondWriterSystem, Access<Write<Position>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.SecondWriter", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, position] : ctx.Query<Position>())
            {
                position.X *= 2.0f;
            }
        }
    };
} // namespace

TEST_CASE("SystemRegistry::RunBefore orders two writers, and AddGroup adds an empty group", "[ecs][system]")
{
    SystemRegistry unordered;
    unordered.Add<SecondWriterSystem>();
    unordered.Add<FirstWriterSystem>();
    REQUIRE_FALSE(unordered.BuildSchedule().Ok());

    Harness h;
    h.Systems.Add<SecondWriterSystem>();
    h.Systems.Add<FirstWriterSystem>();
    h.Systems.RunBefore(SystemOf<FirstWriterSystem>, SystemOf<SecondWriterSystem>);
    h.Systems.AddGroup(LateGroup);
    h.Start();

    const Entity e = h.Storage.Create();
    h.Storage.Add<Position>(e, Position{5.0f});
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Get<Position>(e).X == 2.0f);
}

namespace
{
    struct Score
    {
        static constexpr ComponentInfo Component{.Name = "Owned.Score",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int Points = 7;
    };

    struct ScoreRequests
    {
        static constexpr ComponentInfo Component{
            .Name = "Owned.ScoreRequests", .Kind = ComponentKind::Queue, .Transient = true};
        std::vector<int> Requests;
    };

    /** Made by code outside the systems, like the physics world. */
    struct Device
    {
        static constexpr ComponentInfo Component{.Name = "Owned.Device",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .External = true,
                                                 .Rollback = RollbackClass::Subsystem};
        int Handle = 0;
    };

    /** A singleton with no default constructor: its writer must make it in OnCreate. */
    struct Limits
    {
        static constexpr ComponentInfo Component{.Name = "Owned.Limits",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Keep};
        explicit Limits(const int max) : Max(max) {}
        int Max;
    };

    /** Has no OnCreate. Appends to a queue that another system consumes. */
    class AskSystem final : public System<AskSystem, Access<Append<ScoreRequests>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Owned.Ask", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const { ctx.Queue<ScoreRequests>().Push(1); }
    };

    /** Has no OnCreate for Score or ScoreRequests. Makes Limits, which has no default. */
    class ScoreSystem final
        : public System<ScoreSystem, Access<Write<Score>, Consume<ScoreRequests>, Write<Device>, Write<Limits>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Owned.Score", .In = Order::Simulation};

        void OnCreate(Setup& setup) const { setup.AddSingleton<Limits>(100); }

        void OnDestroy(Setup& setup) const { setup.RemoveSingleton<Limits>(); }

        void FixedUpdate(Context& ctx) const
        {
            for (const int points : ctx.Drain<ScoreRequests>())
            {
                ctx.Singleton<Score>().Points += points;
            }
        }
    };
} // namespace

TEST_CASE("The runner makes the singletons a system reads or writes and the queues it consumes", "[ecs][system]")
{
    STATIC_REQUIRE(Detail::EngineMakes<Write<Score>>());
    STATIC_REQUIRE(Detail::EngineMakes<Consume<ScoreRequests>>());
    STATIC_REQUIRE(Detail::EngineMakes<Read<Score>>());
    STATIC_REQUIRE_FALSE(Detail::EngineMakes<Append<ScoreRequests>>());
    STATIC_REQUIRE_FALSE(Detail::EngineMakes<Write<Device>>());
    STATIC_REQUIRE_FALSE(Detail::EngineMakes<Write<Limits>>());
    STATIC_REQUIRE_FALSE(Detail::EngineMakes<Write<Position>>());

    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<AskSystem>();
    systems.Add<ScoreSystem>();
    ScheduleResult schedule = systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    SystemRunner runner(systems, std::move(schedule.Value), storage);

    // Code outside the systems makes the External one
    storage.SetSingleton<Device>().Handle = 5;

    runner.Create();
    REQUIRE(storage.Singleton<Score>().Points == 7);
    REQUIRE(storage.TryGetSingleton<ScoreRequests>() != nullptr);
    REQUIRE(storage.Singleton<Limits>().Max == 100);
    REQUIRE(storage.Singleton<Device>().Handle == 5);

    runner.RunTick(WorldRole::Play);
    REQUIRE(storage.Singleton<Score>().Points == 8);

    // Destroy removes what Create made, and leaves the External one
    runner.Destroy();
    REQUIRE(storage.TryGetSingleton<Score>() == nullptr);
    REQUIRE(storage.TryGetSingleton<ScoreRequests>() == nullptr);
    REQUIRE(storage.TryGetSingleton<Limits>() == nullptr);
    REQUIRE(storage.Singleton<Device>().Handle == 5);
}

TEST_CASE("The runner keeps a singleton that exists before Create", "[ecs][system]")
{
    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<ScoreSystem>();
    ScheduleResult schedule = systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    SystemRunner runner(systems, std::move(schedule.Value), storage);

    storage.SetSingleton<Device>();
    storage.SetSingleton<Score>().Points = 40;
    runner.Create();
    REQUIRE(storage.Singleton<Score>().Points == 40);

    // It was not the runner's, so the runner does not remove it
    runner.Destroy();
    REQUIRE(storage.Singleton<Score>().Points == 40);
}

namespace
{
    /** A setting that systems read and no system writes. Rollback state. */
    struct Tuning
    {
        static constexpr ComponentInfo Component{.Name = "ReadOnly.Tuning",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int Step = 3;
    };

    /** A Presentation setting that only a frame system reads. */
    struct FrameTuning
    {
        static constexpr ComponentInfo Component{.Name = "ReadOnly.FrameTuning",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
        float Scale = 0.5f;
    };

    struct TunedSum
    {
        static constexpr ComponentInfo Component{.Name = "ReadOnly.TunedSum",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int Value = 0;
    };

    struct TunedShown
    {
        static constexpr ComponentInfo Component{.Name = "ReadOnly.TunedShown",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
        int Step = 0;
        float Scale = 0.0f;
    };

    class TunedStepSystem final : public System<TunedStepSystem, Access<Read<Tuning>, Write<TunedSum>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "ReadOnly.Step", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const { ctx.Singleton<TunedSum>().Value += ctx.Singleton<const Tuning>().Step; }
    };

    class TunedShowSystem final
        : public System<TunedShowSystem, Access<Read<Tuning>, Read<FrameTuning>, Write<TunedShown>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "ReadOnly.Show", .In = Order::Presentation};

        void Update(Context& ctx) const
        {
            TunedShown& shown = ctx.Singleton<TunedShown>();
            shown.Step = ctx.Singleton<const Tuning>().Step;
            shown.Scale = ctx.Singleton<const FrameTuning>().Scale;
        }
    };
} // namespace

TEST_CASE("The runner makes a singleton that no system writes, with its default values, in its domain's storage",
          "[ecs][system]")
{
    STATIC_REQUIRE(Detail::EngineMakes<Read<Tuning>>());
    STATIC_REQUIRE(Detail::EngineMakes<Read<FrameTuning>>());

    Harness h;
    h.Systems.Add<TunedStepSystem>();
    h.Systems.Add<TunedShowSystem>();
    h.Start();
    REQUIRE(h.Storage.TryGetSingleton<Tuning>() != nullptr);
    REQUIRE(h.Storage.TryGetSingleton<FrameTuning>() != nullptr);
    REQUIRE(h.Storage.Singleton<Tuning>().Step == 3);
    REQUIRE(h.Storage.Singleton<FrameTuning>().Scale == 0.5f);

    h.Runner->RunTick(WorldRole::Play);
    h.Runner->RunFrame(0ns, WorldRole::Play);
    REQUIRE(h.Storage.Singleton<TunedSum>().Value == 3);
    REQUIRE(h.Storage.Singleton<TunedShown>().Step == 3);
    REQUIRE(h.Storage.Singleton<TunedShown>().Scale == 0.5f);

    // Code outside the systems sets it; the systems see the new value
    h.Storage.Singleton<Tuning>().Step = 5;
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Singleton<TunedSum>().Value == 8);

    // On a mirror the Presentation one moves with the frame side. The Simulation one stays in
    // the simulation storage: the publish copies it to the mirror.
    EntityStorage mirror;
    h.Runner->SetFrameStorage(mirror);
    REQUIRE(h.Storage.Singleton<Tuning>().Step == 5);
    REQUIRE(h.Storage.TryGetSingleton<FrameTuning>() == nullptr);
    REQUIRE(mirror.TryGetSingleton<Tuning>() == nullptr);
    REQUIRE(mirror.TryGetSingleton<FrameTuning>() != nullptr);
    REQUIRE(mirror.Singleton<FrameTuning>().Scale == 0.5f);

    h.Runner->Destroy();
    REQUIRE(h.Storage.TryGetSingleton<Tuning>() == nullptr);
    REQUIRE(mirror.TryGetSingleton<FrameTuning>() == nullptr);
}

TEST_CASE("A Simulation singleton that only a frame system reads is made in the simulation storage", "[ecs][system]")
{
    // The frame side reads a mirror here; the publish copies the singleton to it
    Harness h;
    h.Systems.Add<TunedShowSystem>();
    EntityStorage mirror;
    ScheduleResult schedule = h.Systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    SystemRunner runner(h.Systems, std::move(schedule.Value), h.Storage);
    runner.SetFrameStorage(mirror);
    runner.Create();
    REQUIRE(h.Storage.TryGetSingleton<Tuning>() != nullptr);
    REQUIRE(h.Storage.Singleton<Tuning>().Step == 3);
    REQUIRE(mirror.TryGetSingleton<Tuning>() == nullptr);
    REQUIRE(mirror.TryGetSingleton<FrameTuning>() != nullptr);
    REQUIRE(mirror.Singleton<FrameTuning>().Scale == 0.5f);
    runner.Destroy();
    REQUIRE(h.Storage.TryGetSingleton<Tuning>() == nullptr);
}

TEST_CASE("The runner keeps a read-only singleton that exists before Create", "[ecs][system]")
{
    Harness h;
    h.Systems.Add<TunedStepSystem>();
    h.Storage.SetSingleton<Tuning>().Step = 9;
    h.Start();
    h.Runner->RunTick(WorldRole::Play);
    REQUIRE(h.Storage.Singleton<TunedSum>().Value == 9);

    h.Runner->Destroy();
    REQUIRE(h.Storage.Singleton<Tuning>().Step == 9);
}

namespace
{
    // The same qualified name as the class in EcsScheduleTests.cpp, so the same SystemTypeId
    class AmbiguousSystem final : public System<AmbiguousSystem, Access<>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.AmbiguousThere", .In = Order::Simulation};

        void FixedUpdate(Context& /*ctx*/) const {}
    };
} // namespace

namespace AmbiguityTest
{
    const SystemDesc& AmbiguousSystemInOtherFile()
    {
        return Detail::DescOf<AmbiguousSystem, AmbiguousSystem::Access>;
    }
} // namespace AmbiguityTest
