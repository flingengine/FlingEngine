// Tests of the runner split (Docs/threading-model.md section 3): the fixed groups run against the
// simulation storage and the variable rate groups against a frame storage, which can be a
// mirror. Each side owns its own singletons.

#include "Ecs/EntityStorage.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"

#include <catch2/catch_all.hpp>
#include <chrono>
#include <memory>

using namespace Fling;
using namespace std::chrono_literals;

namespace
{
    struct SplitPosition
    {
        static constexpr ComponentInfo Component{.Name = "SplitTest.SplitPosition"};
        float X = 0.0f;
    };

    /** What the frame side saw. Owned by the frame system, so it lives in the frame storage. */
    struct SplitShown
    {
        static constexpr ComponentInfo Component{.Name = "SplitTest.SplitShown",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
        float SumX = 0.0f;
        int Frames = 0;
    };

    class SplitMoveSystem final : public System<SplitMoveSystem, Access<Write<SplitPosition>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "SplitTest.Move", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, position] : ctx.Query<SplitPosition>())
            {
                (void)entity;
                position.X += 1.0f;
            }
        }
    };

    class SplitShowSystem final
        : public System<SplitShowSystem, Access<Read<SplitPosition>, Read<RealFrameTime>, Write<SplitShown>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "SplitTest.Show", .In = Order::Presentation};

        void Update(Context& ctx) const
        {
            SplitShown& Shown = ctx.Singleton<SplitShown>();
            Shown.SumX = 0.0f;
            for (auto [entity, position] : ctx.Query<const SplitPosition>())
            {
                (void)entity;
                Shown.SumX += position.X;
            }
            ++Shown.Frames;
        }
    };

    struct SplitRig
    {
        EntityStorage Sim;
        EntityStorage Mirror;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        SplitRig()
        {
            Systems.Add<SplitMoveSystem>();
            Systems.Add<SplitShowSystem>();
            ScheduleResult Schedule = Systems.BuildSchedule();
            REQUIRE(Schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(Schedule.Value), Sim);
        }
    };
} // namespace

TEST_CASE("Runner split: Direct runs both sides on one storage", "[threads][runner]")
{
    SplitRig Rig;
    Rig.Runner->Create();
    REQUIRE(&Rig.Runner->GetFrameStorage() == &Rig.Sim);
    REQUIRE(Rig.Sim.TryGetSingleton<SplitShown>() != nullptr);
    REQUIRE(Rig.Sim.TryGetSingleton<RealFrameTime>() != nullptr);

    Rig.Sim.Create(SplitPosition{0.0f});
    REQUIRE(Rig.Runner->RunFixed(SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 1);
    Rig.Runner->RunFrame(SystemRunnerConfig{}.FixedStep(), WorldRole::Play);
    REQUIRE(Rig.Sim.Singleton<SplitShown>().SumX == 1.0f);
    REQUIRE(Rig.Sim.Singleton<RealFrameTime>().Latest.Tick == 1);
}

TEST_CASE("Runner split: the frame side moves to a mirror and back with its singletons", "[threads][runner]")
{
    SplitRig Rig;
    Rig.Runner->Create();
    Rig.Sim.Create(SplitPosition{0.0f});

    Rig.Runner->SetFrameStorage(Rig.Mirror);
    REQUIRE(&Rig.Runner->GetFrameStorage() == &Rig.Mirror);
    // The frame side's singletons are in the mirror only; the simulation keeps its own
    REQUIRE(Rig.Sim.TryGetSingleton<SplitShown>() == nullptr);
    REQUIRE(Rig.Sim.TryGetSingleton<RealFrameTime>() == nullptr);
    REQUIRE(Rig.Sim.TryGetSingleton<SimTime>() != nullptr);
    REQUIRE(Rig.Mirror.TryGetSingleton<SplitShown>() != nullptr);
    REQUIRE(Rig.Mirror.TryGetSingleton<RealFrameTime>() != nullptr);

    // The ticks change the simulation; the frame reads the mirror and the tick it is told
    Rig.Mirror.Create(SplitPosition{40.0f});
    REQUIRE(Rig.Runner->RunFixed(3 * SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 3);
    TickStamp Shown;
    Shown.Tick = 2;
    Shown.Window.StartNs = 100;
    Shown.Window.EndNs = 200;
    Rig.Runner->RunFrameAt(150, 5ms, WorldRole::Play, Shown);
    REQUIRE(Rig.Mirror.Singleton<SplitShown>().SumX == 40.0f);
    REQUIRE(Rig.Mirror.Singleton<SplitShown>().Frames == 1);
    const RealFrameTime& Frame = Rig.Mirror.Singleton<RealFrameTime>();
    REQUIRE(Frame.Latest.Tick == 2);
    REQUIRE(Frame.Latest.Window.StartNs == 100);
    REQUIRE(Frame.Latest.Window.EndNs == 200);
    REQUIRE(Frame.FrameNs == 150);
    float SimX = 0.0f;
    Rig.Sim.View<SplitPosition>().Each([&](const SplitPosition& t_Position) { SimX += t_Position.X; });
    REQUIRE(SimX == 3.0f);

    // Back to Direct: the singletons move back and are made again
    Rig.Runner->SetFrameStorage(Rig.Sim);
    REQUIRE(Rig.Mirror.TryGetSingleton<SplitShown>() == nullptr);
    REQUIRE(Rig.Mirror.TryGetSingleton<RealFrameTime>() == nullptr);
    REQUIRE(Rig.Sim.Singleton<SplitShown>().Frames == 0);
    Rig.Runner->RunFrame(0ns, WorldRole::Play);
    REQUIRE(Rig.Sim.Singleton<SplitShown>().SumX == 3.0f);

    Rig.Runner->Destroy();
    REQUIRE(Rig.Sim.TryGetSingleton<SplitShown>() == nullptr);
    REQUIRE(Rig.Sim.TryGetSingleton<SimTime>() == nullptr);
}

TEST_CASE("Runner split: a frame storage given before Create gets the frame singletons", "[threads][runner]")
{
    SplitRig Rig;
    Rig.Runner->SetFrameStorage(Rig.Mirror);
    Rig.Runner->Create();
    REQUIRE(Rig.Sim.TryGetSingleton<SplitShown>() == nullptr);
    REQUIRE(Rig.Mirror.TryGetSingleton<SplitShown>() != nullptr);
    Rig.Runner->Destroy();
    REQUIRE(Rig.Mirror.TryGetSingleton<SplitShown>() == nullptr);
    REQUIRE(Rig.Mirror.TryGetSingleton<RealFrameTime>() == nullptr);
}
