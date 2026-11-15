// Tests of stasis (Sandbox.Stasis): a gameplay effect, not input, that stops the look
// camera. The simulation owns it; the frame side sees it only through the published
// LookCamera (Policy.InStasis). The rig is LookRig.h: one input stream through the real
// queue, ticks and frames stepped by hand on a virtual clock.

#include "CameraStatus.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Schedule.h"
#include "Ecs/StateHash.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/TypeRegistry.h"
#include "LookCameraType.h"
#include "LookRig.h"
#include "SimRate.h"
#include "Stasis.h"
#include "StasisType.h"
#include "Systems/CameraStatusSystem.h"
#include "Systems/LookCameraSystem.h"
#include "Systems/StasisSystem.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <span>
#include <vector>

using namespace Fling;
using namespace SandboxTest;

namespace
{
    void AddStasis(SystemRegistry& t_Systems)
    {
        t_Systems.Add<Sandbox::StasisSystem>();
    }

    /**
     * Sandbox.Stasis with the wrong order: the same rule, before Sandbox.LookCamera. The
     * tick that starts the stasis then drops its own input, which the frame side has shown.
     */
    class StasisBeforeLookSystem final : public System<StasisBeforeLookSystem, Sandbox::StasisSystem::Access>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Test.StasisBeforeLook", .In = Order::Simulation, .Before = SystemOf<Sandbox::LookCameraSystem>};

        void FixedUpdate(Context& ctx) const
        {
            const bool pressed = ctx.Singleton<const InputState>().WasPressed(Keys::G);
            const int64 tickNs = ctx.Singleton<const SimTime>().FixedDeltaNs;
            for (auto [entity, stasis, camera] : ctx.Query<Stasis, LookCamera>())
            {
                StasisUtil::Step(stasis, camera.Policy, pressed, tickNs);
            }
        }
    };

    void AddStasisBeforeLook(SystemRegistry& t_Systems)
    {
        t_Systems.Add<StasisBeforeLookSystem>();
    }

    /** What a frame-by-frame run saw. */
    struct FrameRun
    {
        /** The largest step of the presented yaw against the turn of the mouse (backwards). 0: none. */
        float LargestStepBack = 0.0f;
        /** Ticks whose result differs from the view of the frame right before them. */
        uint32 TicksUnlikeShown = 0;
        /** Ticks that did not turn the simulation camera. */
        uint32 StillTicks = 0;
        float YawAtPress = 0.0f;
        float FinalYaw = 0.0f;
    };

    /**
     * Mouse right in every step (the yaw only goes down), with input between the ticks: two
     * moves and frames, then on tick t_PressAt a press of G between them, a third move and
     * frame, then the tick and a frame. Runs past the end of the stasis.
     */
    FrameRun RunFrameByFrame(const std::function<void(SystemRegistry&)>& t_Stasis)
    {
        constexpr uint32 PressAt = 5;
        LookRig Rig(MakeCamera(0.01f), t_Stasis, LookRigConfig{.WithStasis = true});
        FrameRun Out;
        Rig.Tick();
        Rig.Frame();
        float Last = Rig.View().Yaw;
        const auto FrameAndCheck = [&]()
        {
            Rig.Frame();
            const float Now = Rig.View().Yaw;
            Out.LargestStepBack = std::max(Out.LargestStepBack, Now - Last);
            Last = Now;
        };
        for (uint32 t = 0; t < 2 * SimRate::DefaultHz + 20; ++t)
        {
            Rig.Move(2, 0);
            FrameAndCheck();
            Rig.Move(2, 0);
            FrameAndCheck();
            if (t == PressAt)
            {
                Out.YawAtPress = Rig.View().Yaw;
                Rig.Press(Keys::G);
            }
            Rig.Move(2, 0);
            FrameAndCheck();

            const float Shown = Rig.View().Yaw;
            const float SimBefore = Rig.Sim().View.Yaw;
            Rig.Tick();
            Out.TicksUnlikeShown += SameBits(Rig.Sim().View.Yaw, Shown) ? 0 : 1;
            Out.StillTicks += SameBits(Rig.Sim().View.Yaw, SimBefore) ? 1 : 0;
            FrameAndCheck();
        }
        Out.FinalYaw = Rig.Sim().View.Yaw;
        return Out;
    }
} // namespace

TEST_CASE("Stasis: the rule counts down in simulation time and sets the policy flag", "[look][stasis]")
{
    const int64 TickNs = SimRate::StepNs(30);
    REQUIRE(StasisUtil::LengthNs(TickNs) == 60 * TickNs);
    REQUIRE(StasisUtil::LengthNs(SimRate::StepNs(60)) == 120 * SimRate::StepNs(60));
    REQUIRE(StasisUtil::LengthNs(SimRate::StepNs(120)) == 240 * SimRate::StepNs(120));

    Stasis State;
    LookPolicy Policy;
    StasisUtil::Step(State, Policy, false, TickNs);
    REQUIRE_FALSE(Policy.InStasis);
    StasisUtil::Step(State, Policy, true, TickNs);
    REQUIRE(Policy.InStasis);
    REQUIRE(State.RemainingNs == 60 * TickNs);
    for (int i = 0; i < 59; ++i)
    {
        StasisUtil::Step(State, Policy, false, TickNs);
    }
    REQUIRE(Policy.InStasis);
    REQUIRE(State.RemainingNs == TickNs);
    StasisUtil::Step(State, Policy, false, TickNs);
    REQUIRE_FALSE(Policy.InStasis);
    REQUIRE(State.RemainingNs == 0);

    // A press during stasis starts it again; the player's freeze is a separate flag
    Policy.Frozen = true;
    StasisUtil::Step(State, Policy, true, TickNs);
    StasisUtil::Step(State, Policy, false, TickNs);
    StasisUtil::Step(State, Policy, true, TickNs);
    REQUIRE(State.RemainingNs == 60 * TickNs);
    REQUIRE(Policy.Frozen);
}

TEST_CASE("Stasis: lasts the same simulated time at 30, 60 and 120 Hz", "[look][stasis]")
{
    for (const uint32 Hz : {30u, 60u, 120u})
    {
        INFO(Hz << " Hz");
        LookRig Rig(MakeCamera(0.01f), AddStasis, LookRigConfig{.SimHz = Hz, .WithStasis = true});
        Rig.Tick();
        const int64 TickNs = Rig.Storage.Singleton<SimTime>().FixedDeltaNs;
        REQUIRE(TickNs == SimRate::StepNs(Hz));

        // The tick that takes the press still turns by its own move, then sets the flag
        Rig.Press(Keys::G);
        Rig.Move(1, 0);
        Rig.Tick();
        REQUIRE(Rig.Sim().Policy.InStasis);
        REQUIRE(Rig.Sim().View.Yaw == Catch::Approx(-0.01f));
        const float Held = Rig.Sim().View.Yaw;

        // One move per tick: count the ticks that drop it, up to the first that turns
        uint32 Dropped = 0;
        for (uint32 i = 0; i < 4 * Hz; ++i)
        {
            Rig.Move(1, 0);
            Rig.Tick();
            if (!SameBits(Rig.Sim().View.Yaw, Held))
            {
                break;
            }
            ++Dropped;
        }
        REQUIRE_FALSE(Rig.Sim().Policy.InStasis);
        REQUIRE(Rig.Sim().View.Yaw == Catch::Approx(Held - 0.01f));
        // 2 s of simulated time: twice the rate in ticks
        REQUIRE(Dropped == 2 * Hz);
        REQUIRE(static_cast<int64>(Dropped) * TickNs == StasisUtil::LengthNs(TickNs));
    }
}

TEST_CASE("Stasis: mouse input during stasis turns neither the simulation camera nor the presented view",
          "[look][stasis]")
{
    LookRig Rig(MakeCamera(0.01f), AddStasis, LookRigConfig{.WithStasis = true});
    Rig.Tick();
    Rig.Press(Keys::G);
    Rig.Tick();
    REQUIRE(Rig.Sim().Policy.InStasis);
    const LookView Held = Rig.Sim().View;

    size_t MostPending = 0;
    for (int i = 0; i < 24; ++i)
    {
        Rig.Move(7, -5);
        Rig.Frame();
        MostPending = std::max(MostPending, Rig.PendingSamples());
        REQUIRE(SameBits(Rig.View(), Held));
        if (i % 3 == 2)
        {
            Rig.Tick();
            REQUIRE(Rig.Sim().Policy.InStasis);
            REQUIRE(SameBits(Rig.Sim().View, Held));
        }
    }
    // The frames had the moves pending, and dropped them by the published flag
    REQUIRE(MostPending == 3);
}

TEST_CASE("Stasis: the presented view never moves backwards at the start or the end", "[look][stasis]")
{
    SECTION("After Sandbox.LookCamera (the Sandbox's order): every frame turns on, every tick makes what was shown")
    {
        const FrameRun Run = RunFrameByFrame(AddStasis);
        CHECK(Run.LargestStepBack == 0.0f);
        CHECK(Run.TicksUnlikeShown == 0);
        // The stasis happened (60 ticks at 30 Hz with no turn), and the camera turned again after it
        CHECK(Run.StillTicks == 2 * SimRate::DefaultHz);
        CHECK(Run.FinalYaw < Run.YawAtPress - 0.1f);
    }

    SECTION("Before Sandbox.LookCamera: the tick of the press drops input that the frames showed, a snap back")
    {
        const FrameRun Run = RunFrameByFrame(AddStasisBeforeLook);
        // The 6 counts of that tick (two moves before the press, one after it), at 0.01 rad per count
        CHECK(Run.LargestStepBack == Catch::Approx(0.06f));
        // That tick, and the last tick of stasis, which turns by input that the frames dropped (a jump forward)
        CHECK(Run.TicksUnlikeShown == 2);
    }
}

TEST_CASE("Stasis: with a late publish, input after the cut of the tick that starts or ends it is shown wrongly",
          "[look][stasis]")
{
    // Threaded play publishes a tick after it ran. A frame in between still shows the tick
    // before, with the input after that tick's cut pending, under the old policy. The frame
    // side cannot know a gameplay decision before it sees the tick, so the input that came in
    // between the cut and the publish is shown wrongly for those frames: taken back at the
    // start, shown late at the end. Nothing else is.
    LookRig Rig(MakeCamera(0.01f), AddStasis, LookRigConfig{.WithStasis = true, .Mirror = true});
    Rig.Tick();
    Rig.Frame();

    SECTION("Start: the input between the cut and the publish is taken back")
    {
        Rig.Move(5, 0);
        Rig.Press(Keys::G);
        Rig.TickUnpublished();
        Rig.Move(10, 0);
        Rig.Frame();
        REQUIRE(Rig.Shown().Tick == 1);
        REQUIRE(Rig.View().Yaw == Catch::Approx(-0.15f));

        Rig.Publish();
        Rig.Frame();
        REQUIRE(Rig.Shown().Tick == 2);
        // The 10 counts after the cut of tick 2: 0.1 rad back
        REQUIRE(Rig.View().Yaw == Catch::Approx(-0.05f));
        Rig.Tick();
        REQUIRE(SameBits(Rig.Sim().View.Yaw, Rig.View().Yaw));
    }

    SECTION("End: the input between the cut and the publish is shown late, never backwards")
    {
        Rig.Press(Keys::G);
        Rig.Tick();
        Rig.Frame();
        const float Held = Rig.View().Yaw;
        // Up to the tick before the last tick of stasis
        while (Rig.Storage.Get<Stasis>(Rig.Camera).RemainingNs > SimRate::StepNs(SimRate::DefaultHz))
        {
            Rig.Tick();
        }
        Rig.Move(5, 0);
        Rig.TickUnpublished();
        REQUIRE_FALSE(Rig.Sim().Policy.InStasis);
        Rig.Move(10, 0);
        Rig.Frame();
        REQUIRE(SameBits(Rig.View().Yaw, Held));

        Rig.Publish();
        Rig.Frame();
        // The 5 counts went to the last tick of stasis; the 10 after its cut turn now
        REQUIRE(Rig.View().Yaw == Catch::Approx(Held - 0.1f));
        Rig.Tick();
        REQUIRE(SameBits(Rig.Sim().View.Yaw, Rig.View().Yaw));
    }
}

TEST_CASE("Stasis: F and G compose", "[look][stasis]")
{
    LookRig Rig(MakeCamera(0.01f), AddStasis, LookRigConfig{.WithStasis = true});
    Rig.Tick();
    const auto RunPastStasis = [&Rig]()
    {
        for (uint32 i = 0; i <= 2 * SimRate::DefaultHz && Rig.Sim().Policy.InStasis; ++i)
        {
            Rig.Move(3, 0);
            Rig.Frame();
            Rig.Tick();
        }
        REQUIRE_FALSE(Rig.Sim().Policy.InStasis);
    };

    SECTION("Frozen during stasis stays frozen after it")
    {
        Rig.Press(Keys::G);
        Rig.Tick();
        const LookView Held = Rig.Sim().View;
        Rig.Press(Keys::F);
        Rig.Move(4, 0);
        Rig.Frame();
        REQUIRE(SameBits(Rig.View(), Held));
        RunPastStasis();
        REQUIRE(Rig.Sim().Policy.Frozen);
        Rig.Move(9, 0);
        Rig.Frame();
        REQUIRE(SameBits(Rig.View(), Held));
        Rig.Tick();
        REQUIRE(SameBits(Rig.Sim().View, Held));

        // F again: the player's freeze ends, and the camera turns on both sides
        Rig.Press(Keys::F);
        Rig.Move(9, 0);
        Rig.Frame();
        REQUIRE(Rig.View().Yaw == Catch::Approx(Held.Yaw - 0.09f));
        Rig.Tick();
        REQUIRE(SameBits(Rig.Sim().View.Yaw, Rig.View().Yaw));
    }

    SECTION("Frozen before stasis stays frozen after it")
    {
        Rig.Press(Keys::F);
        Rig.Tick();
        Rig.Press(Keys::G);
        Rig.Tick();
        REQUIRE(Rig.Sim().Policy.Frozen);
        REQUIRE(Rig.Sim().Policy.InStasis);
        const LookView Held = Rig.Sim().View;
        RunPastStasis();
        REQUIRE(Rig.Sim().Policy.Frozen);
        REQUIRE(SameBits(Rig.Sim().View, Held));
    }

    SECTION("Unfrozen during stasis: still no turn until the stasis ends")
    {
        Rig.Press(Keys::F);
        Rig.Tick();
        Rig.Press(Keys::G);
        Rig.Tick();
        const LookView Held = Rig.Sim().View;
        Rig.Press(Keys::F);
        Rig.Move(6, 0);
        Rig.Frame();
        REQUIRE(SameBits(Rig.View(), Held));
        Rig.Tick();
        REQUIRE_FALSE(Rig.Sim().Policy.Frozen);
        REQUIRE(SameBits(Rig.Sim().View, Held));
        RunPastStasis();
        REQUIRE(SameBits(Rig.Sim().View, Held));
        Rig.Move(6, 0);
        Rig.Frame();
        REQUIRE(Rig.View().Yaw == Catch::Approx(Held.Yaw - 0.06f));
    }
}

TEST_CASE("Stasis: the camera status for the UI comes from published state", "[look][stasis]")
{
    LookRig Rig(
        MakeCamera(0.01f),
        [](SystemRegistry& t_Systems)
        {
            t_Systems.Add<Sandbox::StasisSystem>();
            t_Systems.Add<Sandbox::CameraStatusSystem>();
        },
        LookRigConfig{.WithStasis = true});
    const auto Status = [&Rig]()
    {
        return Rig.FrameStorage().Singleton<CameraStatus>();
    };
    Rig.Tick();
    Rig.Frame();
    REQUIRE(Status().Mode == CameraMode::Free);

    Rig.Press(Keys::G);
    Rig.Frame();
    REQUIRE(Status().Mode == CameraMode::Free);
    Rig.Tick();
    Rig.Frame();
    REQUIRE(Status().Mode == CameraMode::Stasis);
    REQUIRE(Status().StasisSecondsLeft == Catch::Approx(2.0f).margin(1e-6));
    for (int i = 0; i < 18; ++i)
    {
        Rig.Tick();
    }
    Rig.Frame();
    REQUIRE(Status().StasisSecondsLeft == Catch::Approx(1.4f).margin(1e-6));

    // F during stasis: stasis shows first, then the freeze stays
    Rig.Press(Keys::F);
    Rig.Tick();
    Rig.Frame();
    REQUIRE(Status().Mode == CameraMode::Stasis);
    while (Rig.Sim().Policy.InStasis)
    {
        Rig.Tick();
    }
    Rig.Frame();
    REQUIRE(Status().Mode == CameraMode::Frozen);
}

TEST_CASE("Stasis: the state hash covers Stasis and the InStasis flag", "[look][stasis]")
{
    SystemRegistry Systems;
    ModuleRegistration Modules(Systems);
    Sandbox::RegisterLookCameraType(Modules);
    Sandbox::RegisterStasisType(Modules);
    const TypeRegistry Types = TypeRegistry::Build(Modules);
    REQUIRE(Types.Ok());

    EntityStorage State;
    const Entity Camera = State.Create(LookCamera{}, Stasis{});
    const uint64 Start = HashSimulationState(State, Types);
    State.Get<Stasis>(Camera).RemainingNs = 1;
    const uint64 Counting = HashSimulationState(State, Types);
    REQUIRE(Counting != Start);
    State.Get<LookCamera>(Camera).Policy.InStasis = true;
    REQUIRE(HashSimulationState(State, Types) != Counting);
}

TEST_CASE("Stasis: the schedule needs its order against Sandbox.LookCamera", "[look][stasis][schedule]")
{
    const Sandbox::LookCameraSystem Look{};
    const Sandbox::StasisSystem Real{};

    SECTION("Without .After: two writers of LookCamera are a write conflict, and the fix names the class")
    {
        SystemDesc Unordered = Real.Describe();
        Unordered.Info.After = OrderList{};
        const SystemDesc* Descs[] = {&Look.Describe(), &Unordered};
        const ScheduleResult Result = BuildSchedule(std::span<const SystemDesc* const>(Descs));
        REQUIRE(Result.Errors.size() == 1);
        REQUIRE(Result.Errors[0].Kind == ScheduleErrorKind::WriteConflict);
        INFO(Result.Errors[0].Message);
        REQUIRE(Result.Errors[0].Message.find("Sandbox.LookCamera and Sandbox.Stasis both write LookCamera") !=
                String::npos);
        REQUIRE(Result.Errors[0].Message.find(
                    "Fix: add .After = Fling::SystemOf<Sandbox::LookCameraSystem> to Sandbox.Stasis's SystemInfo") !=
                String::npos);
    }

    SECTION("With it: Sandbox.LookCamera runs first, through the stated order")
    {
        SystemRegistry Systems;
        Systems.Add<Sandbox::LookCameraSystem>();
        Systems.Add<Sandbox::StasisSystem>();
        const ScheduleResult Result = Systems.BuildSchedule();
        REQUIRE(Result.ErrorReport() == "");
        std::vector<String> Fixed;
        for (const uint32 i : Result.Value.RunOrder(UpdateRate::Fixed))
        {
            Fixed.emplace_back(Result.Value.Systems[i]->Info.Name);
        }
        REQUIRE(Fixed == std::vector<String>{"Sandbox.LookCamera", "Sandbox.Stasis"});
        REQUIRE(Result.Value.Edges.size() == 1);
        REQUIRE(Result.Value.Edges[0].Reason == EdgeReason::After);
        const String Dump = Result.Value.Dump();
        INFO(Dump);
        REQUIRE(Dump.find("after Sandbox.LookCamera: .After") != String::npos);
    }
}
