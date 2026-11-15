// The input boundary of the simulation. Input reaches a system only as data that it
// declares: the runner writes the tick's input into InputState before the fixed systems,
// and the input after the shown cut into PendingInput before the frame systems
// (SystemRunner::RunOneTick, RunFrameAt). No system can write either one, and the system
// context has no input of its own (ECS compile checks WritesInputState, WritesPendingInput,
// ReadsTickInput, ReadsPendingInput). A fixed system that reads the frame's input from the
// Input singleton instead fails the simulation check, and on the game thread the lane check.
// The same simulation check guards the clocks and the global random stream.

#include "../Common/TestScopes.h"
#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Input/Input.h"
#include "Misc/SimulationCheck.h"
#include "PlatformTime.h"
#include "Random.h"
#include "Threading/Lane.h"
#include "Timing.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <functional>
#include <utility>
#include <vector>

using namespace Fling;
using namespace std::chrono_literals;

namespace
{
    // Systems have no state, so what they do and see is global to this file
    std::function<void()> g_Call;

    struct Seen
    {
        uint64 Tick = 0;
        InputCut Cut;
        size_t MouseSamples = 0;
    };
    std::vector<Seen> g_TickSeen;
    std::vector<Seen> g_FrameSeen;

    /** A component that FixedCallSystem's deferred change writes. */
    struct DeferProbe
    {
        static constexpr ComponentInfo Component{.Name = "Test.DeferProbe"};
        int32 Value = 0;
    };

    /** A fixed system that calls g_Call, and g_DeferredCall in a change at the end of the tick: simulation code. */
    std::function<void()> g_DeferredCall;
    class FixedCallSystem final : public System<FixedCallSystem, Access<Read<SimTime>, Write<DeferProbe>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.FixedCall", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            if (g_Call)
            {
                g_Call();
            }
            if (g_DeferredCall)
            {
                ctx.Defer<DeferProbe>([](EntityStorage&) { g_DeferredCall(); });
            }
        }
    };

    /** A frame system that calls g_Call: frame code, which may read the Input singleton. */
    class FrameCallSystem final : public System<FrameCallSystem, Access<Read<RealFrameTime>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.FrameCall", .In = Order::Presentation};

        void Update(Context&) const
        {
            if (g_Call)
            {
                g_Call();
            }
        }
    };

    /** Records the InputState that each tick sees. */
    class TickInputReader final : public System<TickInputReader, Access<Read<SimTime>, Read<InputState>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.TickInputReader", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const InputState& input = ctx.Singleton<const InputState>();
            g_TickSeen.push_back(
                Seen{ctx.Singleton<const SimTime>().Tick, input.Tick.Slice.Cut, input.Tick.Slice.MouseSamples.size()});
        }
    };

    /** Records the PendingInput that each frame sees. */
    class PendingInputReaderSystem final
        : public System<PendingInputReaderSystem, Access<Read<RealFrameTime>, Read<PendingInput>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.PendingInputReader", .In = Order::Presentation};

        void Update(Context& ctx) const
        {
            const PendingInput& pending = ctx.Singleton<const PendingInput>();
            g_FrameSeen.push_back(
                Seen{ctx.Singleton<const RealFrameTime>().Latest.Tick, pending.Cut, pending.MouseSamples.size()});
        }
    };

    struct NamedCall
    {
        const char* Name;
        std::function<void()> Call;
    };

#if FLING_SIMULATION_CHECKS
    std::vector<NamedCall> FrameInputCalls()
    {
        return {
            {"IsKeyboardCaptured",
             []
             {
                 (void)Input::Get().IsKeyboardCaptured();
             }},
            {"IsMouseCaptured",
             []
             {
                 (void)Input::Get().IsMouseCaptured();
             }},
            {"GetDevice",
             []
             {
                 (void)Input::Get().GetDevice(InputDeviceId{});
             }},
            {"GetDevices",
             []
             {
                 (void)Input::Get().GetDevices();
             }},
            {"CreateProcessor",
             []
             {
                 (void)Input::Get().CreateProcessor();
             }},
            {"GetHandler",
             []
             {
                 (void)Input::Get().GetHandler();
             }},
        };
    }
#endif

    /** One runner with the call systems. */
    struct CallRig
    {
        CallRig()
        {
            Systems.Add<FixedCallSystem>();
            Systems.Add<FrameCallSystem>();
            ScheduleResult Built = Systems.BuildSchedule();
            REQUIRE(Built.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(Built.Value), Storage);
            Runner->Create();
        }

        SystemRegistry Systems;
        EntityStorage Storage;
        std::unique_ptr<SystemRunner> Runner;
    };
} // namespace

TEST_CASE("Input boundary: a fixed system that reads the Input singleton fails the simulation check",
          "[input][simulation]")
{
#if FLING_SIMULATION_CHECKS
    const TestScopes::CountSimulationViolations SimulationViolations;
    const TestScopes::CountLaneViolations LaneViolations;
    const TestScopes::InputScope InputOn;
    CallRig Rig;
    for (const NamedCall& Each : FrameInputCalls())
    {
        CAPTURE(Each.Name);
        g_Call = Each.Call;

        // The frame side may read it
        TestScopes::CountSimulationViolations::Count = 0;
        Rig.Runner->RunFrame(16ms, WorldRole::Play);
        REQUIRE(TestScopes::CountSimulationViolations::Count == 0);

        // A fixed system may not: one tick runs, the check fires
        REQUIRE(Rig.Runner->RunFixed(SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 1);
        REQUIRE(TestScopes::CountSimulationViolations::Count > 0);
    }
    g_Call = {};
#else
    SUCCEED("Simulation checks are compiled out in this build");
#endif
}

TEST_CASE("Simulation check: a fixed system that reads a clock or the global Random fails it", "[simulation]")
{
#if FLING_SIMULATION_CHECKS
    // One call per guard (FLING_SIMULATION_FORBID), so a guard that is taken out fails here
    const std::vector<NamedCall> Guarded = {
        {"PlatformTime::NowNanoseconds",
         []
         {
             (void)PlatformTime::NowNanoseconds();
         }},
        {"Timing::GetDeltaTime",
         []
         {
             (void)Timing::Get().GetDeltaTime();
         }},
        {"Timing::GetTime",
         []
         {
             (void)Timing::Get().GetTime();
         }},
        {"Random::Random0ToN",
         []
         {
             (void)Random::Random0ToN(10);
         }},
    };
    const TestScopes::CountSimulationViolations SimulationViolations;
    const TestScopes::CountLaneViolations LaneViolations;
    const uint32 Seed = Random::GetSeed();
    Random::Init(1);
    CallRig Rig;
    for (const NamedCall& Each : Guarded)
    {
        CAPTURE(Each.Name);

        // Frame code may call it
        g_Call = Each.Call;
        TestScopes::CountSimulationViolations::Count = 0;
        Rig.Runner->RunFrame(16ms, WorldRole::Play);
        REQUIRE(TestScopes::CountSimulationViolations::Count == 0);

        // In a fixed system, and in a change it defers to the end of the tick, it fails
        REQUIRE(Rig.Runner->RunFixed(SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 1);
        REQUIRE(TestScopes::CountSimulationViolations::Count == 1);
        g_Call = {};
        g_DeferredCall = Each.Call;
        TestScopes::CountSimulationViolations::Count = 0;
        REQUIRE(Rig.Runner->RunFixed(SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 1);
        REQUIRE(TestScopes::CountSimulationViolations::Count == 1);
        g_DeferredCall = {};
    }
    Random::Init(Seed);
#else
    SUCCEED("Simulation checks are compiled out in this build");
#endif
}

TEST_CASE("Input boundary: the runner writes InputState before the fixed systems and PendingInput before the frame",
          "[input][simulation]")
{
    g_TickSeen.clear();
    g_FrameSeen.clear();
    SystemRegistry Systems;
    Systems.Add<TickInputReader>();
    Systems.Add<PendingInputReaderSystem>();
    ScheduleResult Built = Systems.BuildSchedule();
    REQUIRE(Built.ErrorReport() == "");
    // No system writes them, so nothing orders the readers: the runner writes first
    REQUIRE(Built.Value.Edges.empty());
    EntityStorage Storage;
    SystemRunner Runner(Systems, std::move(Built.Value), Storage);

    // Tick n takes the messages (10 (n - 1), 10 n] with n mouse samples
    uint32 Builds = 0;
    Runner.SetTickInputSource(
        [&Builds](TickInput& t_Out, const TickWindow&)
        {
            ++Builds;
            t_Out.Clear();
            t_Out.Slice.Cut = InputCut{10 * (Builds - 1), 10 * Builds};
            t_Out.Slice.MouseSamples.assign(Builds, InputMouseSample{});
        });
    // The pending input after a cut is the next three messages
    std::vector<uint64> PendingAfter;
    Runner.SetPendingInputSource(
        [&PendingAfter](const uint64 t_After, InputSlice& t_Out)
        {
            PendingAfter.push_back(t_After);
            t_Out.Clear();
            t_Out.Cut = InputCut{t_After, t_After + 3};
            t_Out.MouseSamples.assign(3, InputMouseSample{});
        });

    REQUIRE(Storage.TryGetSingleton<InputState>() == nullptr);
    Runner.Create();
    REQUIRE(Storage.TryGetSingleton<InputState>() != nullptr);
    REQUIRE(Storage.TryGetSingleton<PendingInput>() != nullptr);

    // Before the first tick there is no cut: nothing is pending, and the source is not asked
    Runner.RunFrame(16ms, WorldRole::Play);
    REQUIRE(g_FrameSeen.size() == 1);
    CHECK(g_FrameSeen[0].Tick == 0);
    CHECK(g_FrameSeen[0].Cut == InputCut{0, 0});
    CHECK(g_FrameSeen[0].MouseSamples == 0);
    CHECK(PendingAfter.empty());

    // Each tick sees the input the source made for it, and the ledger has the same cut
    REQUIRE(Runner.RunFixed(3 * SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 3);
    REQUIRE(Builds == 3);
    REQUIRE(g_TickSeen.size() == 3);
    for (uint64 t = 1; t <= 3; ++t)
    {
        CAPTURE(t);
        const Seen& Tick = g_TickSeen[t - 1];
        CHECK(Tick.Tick == t);
        CHECK(Tick.Cut == InputCut{10 * (t - 1), 10 * t});
        CHECK(Tick.MouseSamples == t);
        const TickRecord* Record = Runner.GetLedger().Find(t);
        REQUIRE(Record != nullptr);
        CHECK(Record->Stamp.Cut == Tick.Cut);
        CHECK(Record->MouseSamples == t);
    }

    // The frame shows tick 3: its pending input starts after tick 3's cut
    Runner.RunFrame(16ms, WorldRole::Play);
    REQUIRE(g_FrameSeen.size() == 2);
    CHECK(g_FrameSeen[1].Tick == 3);
    CHECK(PendingAfter == std::vector<uint64>{30});
    CHECK(g_FrameSeen[1].Cut == InputCut{30, 33});
    CHECK(g_FrameSeen[1].MouseSamples == 3);

    // With no source a tick has no input, not the last tick's
    Runner.SetTickInputSource({});
    REQUIRE(Runner.RunFixed(SystemRunnerConfig{}.FixedStep(), WorldRole::Play) == 1);
    REQUIRE(g_TickSeen.size() == 4);
    CHECK(g_TickSeen[3].Cut == InputCut{});
    CHECK(g_TickSeen[3].MouseSamples == 0);

    Runner.Destroy();
    REQUIRE(Storage.TryGetSingleton<InputState>() == nullptr);
    REQUIRE(Storage.TryGetSingleton<PendingInput>() == nullptr);
}
