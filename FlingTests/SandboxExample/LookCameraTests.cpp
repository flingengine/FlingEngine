// Tests of the late look: one input stream, a fixed tick that consumes it (the runner
// writes InputState, Sandbox.LookCamera reads it), and a frame side that shows the input
// the tick has not consumed yet (the runner writes PendingInput, Sandbox.LookCameraView
// reads it), from scratch each frame. These test the Sandbox example game, not the engine:
// the look camera sources use the Engine module only, so SandboxExampleTests compiles them
// without the rest of the Sandbox (FlingTests/CMakeLists.txt).
//
// The rig (LookRig.h) puts input through the real queue: a VirtualInputDeviceInterface on
// an InputMessageHandler, the engine's TickInputProvider for the ticks, PendingInputReader
// for the frames, all on a virtual clock.

#include "Components/CameraView.h"
#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Input/InputMessageHandler.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Interpolate.h"
#include "LookCamera.h"
#include "LookRig.h"
#include "LookUtil.h"
#include "PlatformTime.h"
#include "PresentationLink.h"
#include "Systems/LookCameraSystem.h"
#include "Systems/LookCameraViewSystem.h"

#include <bit>
#include <catch2/catch_all.hpp>
#include <cstring>
#include <functional>
#include <memory>
#include <span>
#include <vector>

using namespace Fling;
using namespace SandboxTest;

TEST_CASE("LookUtil turns and clamps, and does not depend on the split", "[look]")
{
    const LookPolicy Policy{.LookScale = 0.01f, .PitchMin = -1.0f, .PitchMax = 1.0f};
    const LookView Turned = LookUtil::Detail::Apply(LookView{}, 10, -20, Policy);
    REQUIRE(Turned.Yaw == Catch::Approx(-0.1f));
    REQUIRE(Turned.Pitch == Catch::Approx(0.2f));
    REQUIRE(LookUtil::Detail::Apply(LookView{}, 0, -1000, Policy).Pitch == 1.0f);

    LookPolicy Frozen = Policy;
    Frozen.Frozen = true;
    REQUIRE(LookUtil::Detail::Apply(LookView{0.5f, 0.25f}, 100, 100, Frozen) == LookView{0.5f, 0.25f});

    const std::vector<InputMouseSample> All{{0, 3, -70}, {0, 7, 50}, {0, -11, 90}, {0, 5, -30}};
    const std::vector<InputMouseSample> First(All.begin(), All.begin() + 2);
    const std::vector<InputMouseSample> Rest(All.begin() + 2, All.end());
    const LookView Split = LookUtil::Detail::Look(LookUtil::Detail::Look(LookView{}, First, Policy), Rest, Policy);
    REQUIRE(SameBits(Split, LookUtil::Detail::Look(LookView{}, All, Policy)));
}

TEST_CASE("Late look: no input is lost and none is counted twice", "[look]")
{
    LookRig Rig(MakeCamera(0.0025f));

    SECTION("Several frames per tick")
    {
        Rig.Tick();
        Rig.Frame();
        Rig.CheckFrame();
        Rig.Move(5, -3);
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.PendingSamples() == 1);
        REQUIRE_FALSE(SameBits(Rig.View(), Rig.Sim().View));
        Rig.Move(7, 2);
        Rig.Move(-1, 4);
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.PendingSamples() == 3);
        Rig.Frame();
        Rig.CheckFrame();

        // The frames read the queue and consumed nothing: the tick gets all three
        const LookView ShownBeforeTick = Rig.View();
        Rig.Tick();
        Rig.CheckSimHasAll();
        REQUIRE(SameBits(Rig.Sim().View, ShownBeforeTick));
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.PendingSamples() == 0);
        REQUIRE(SameBits(Rig.View(), Rig.Sim().View));
    }

    SECTION("Several ticks in one frame (a hitch)")
    {
        Rig.Tick();
        Rig.Move(10, 0);
        Rig.Frame();
        Rig.CheckFrame();
        Rig.Move(20, -5);
        Rig.Move(-3, 9);
        Rig.Tick();
        Rig.Tick();
        Rig.Tick();
        Rig.CheckSimHasAll();
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.PendingSamples() == 0);
        REQUIRE(SameBits(Rig.View(), Rig.Sim().View));
    }

    SECTION("Frames with no tick and no input")
    {
        Rig.Move(4, 4);
        Rig.Tick();
        for (int i = 0; i < 5; ++i)
        {
            Rig.Frame();
            Rig.CheckFrame();
            REQUIRE(SameBits(Rig.View(), Rig.Sim().View));
        }
    }

    SECTION("Input that arrives between a tick and the frame is shown by that frame")
    {
        Rig.Tick();
        Rig.Move(12, -6);
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.PendingSamples() == 1);
        REQUIRE(Rig.View().Yaw == Catch::Approx(Rig.Sim().View.Yaw - 12 * 0.0025f));
        Rig.Tick();
        Rig.CheckSimHasAll();
    }

    SECTION("Before the first tick, nothing is pending")
    {
        Rig.Move(12, -6);
        Rig.Frame();
        REQUIRE(Rig.PendingSamples() == 0);
        REQUIRE(Rig.Shown().Tick == 0);
    }
}

namespace
{
    /** One step of an input script: a mouse move, a tick, or n frames. */
    struct Step
    {
        enum Kind
        {
            Move,
            Tick,
        } What;
        int32 Dx = 0;
        int32 Dy = 0;
    };

    /**
     * The same moves and ticks with a different number of frames between the steps.
     * Returns the sim view after each tick.
     */
    std::vector<LookView> RunScript(const std::vector<Step>& t_Script, const int t_FramesPerStep)
    {
        LookRig Rig(MakeCamera(0.0031f, 1.2f));
        std::vector<LookView> Views;
        for (const Step& Each : t_Script)
        {
            if (Each.What == Step::Move)
            {
                Rig.Move(Each.Dx, Each.Dy);
            }
            else
            {
                Rig.Tick();
                Views.push_back(Rig.Sim().View);
            }
            for (int i = 0; i < t_FramesPerStep; ++i)
            {
                Rig.Frame();
                Rig.CheckFrame();
            }
        }
        return Views;
    }

    /**
     * Frames tied to input, the way the engine loop works today: input is polled at the
     * start of each frame, then the ticks that are due run, then the frame systems.
     * Moves happen at fixed times (microseconds); the frame rate changes which tick
     * takes which move. Returns the final sim view, after every move is consumed.
     */
    LookView RunAtFrameRate(const int t_FramesPerSecond)
    {
        LookRig Rig(MakeCamera(0.0031f, 1.2f));
        // A fast flick up past the clamp (1.5) and back, with some yaw, over 100 ms
        struct TimedMove
        {
            int64 AtUs;
            int32 Dx;
            int32 Dy;
        };
        std::vector<TimedMove> Moves;
        for (int i = 0; i < 25; ++i)
        {
            const int32 Dy = i < 12 ? -17 : (i < 24 ? 15 : 0);
            Moves.push_back(TimedMove{i * 4000 + 1000, 3 + (i % 5), Dy});
        }

        const int64 FrameUs = 1'000'000 / t_FramesPerSecond;
        const int64 TickUs = 16'667;
        int64 Accumulator = 0;
        size_t Next = 0;
        for (int64 Now = 0; Now < 200'000; Now += FrameUs)
        {
            while (Next < Moves.size() && Moves[Next].AtUs <= Now)
            {
                Rig.QueueMove(Moves[Next].Dx, Moves[Next].Dy);
                ++Next;
            }
            Rig.Poll();
            Accumulator += FrameUs;
            while (Accumulator >= TickUs)
            {
                Rig.Tick();
                Accumulator -= TickUs;
            }
            Rig.Frame();
            Rig.CheckFrame();
        }
        Rig.Tick();
        // The ticks together give the fold of every move
        Rig.CheckSimHasAll();
        return Rig.Sim().View;
    }
} // namespace

TEST_CASE("Late look: the sim view does not depend on the frames", "[look]")
{
    std::vector<Step> Script;
    for (int i = 0; i < 40; ++i)
    {
        Script.push_back(Step{Step::Move, 3 + i % 7, (i % 9) - 4 - (i < 10 ? 20 : 0)});
        if (i % 3 == 2)
        {
            Script.push_back(Step{Step::Tick});
        }
        if (i % 11 == 10)
        {
            // A hitch: three ticks in a row
            Script.push_back(Step{Step::Tick});
            Script.push_back(Step{Step::Tick});
        }
    }
    Script.push_back(Step{Step::Tick});

    SECTION("Input on its own clock: frames change nothing in the simulation")
    {
        const std::vector<LookView> NoFrames = RunScript(Script, 0);
        for (const int Frames : {1, 2, 5})
        {
            const std::vector<LookView> WithFrames = RunScript(Script, Frames);
            REQUIRE(WithFrames.size() == NoFrames.size());
            for (size_t i = 0; i < NoFrames.size(); ++i)
            {
                REQUIRE(SameBits(WithFrames[i], NoFrames[i]));
            }
        }
    }

    SECTION("Input polled once per frame: the same camera at every frame rate")
    {
        // A flick past the clamp: clamping after each sample makes it independent of which tick took which move
        const LookView At30 = RunAtFrameRate(30);
        for (const int Fps : {60, 144, 240})
        {
            REQUIRE(SameBits(RunAtFrameRate(Fps), At30));
        }
        REQUIRE(At30.Pitch == Catch::Approx(0.942f).margin(1e-5));
    }
}

TEST_CASE("Late look: a freeze press takes effect at its place in the input", "[look]")
{
    LookRig Rig(MakeCamera(0.01f));
    Rig.Move(10, 0);
    Rig.Tick();
    REQUIRE(Rig.Sim().View.Yaw == Catch::Approx(-0.1f));

    // Moves on both sides of the press, before the tick that consumes them. The frame side
    // runs the same rule as the tick: the 5 counts before F turn the view, the 7 after it do not.
    Rig.Move(5, 0);
    Rig.Frame();
    Rig.CheckFrame();
    const float BeforePress = Rig.View().Yaw;
    REQUIRE(BeforePress == Catch::Approx(-0.15f));
    Rig.Press(Keys::F);
    Rig.Move(7, 0);
    Rig.Frame();
    Rig.CheckFrame();
    REQUIRE(SameBits(Rig.View().Yaw, BeforePress));

    // The tick makes exactly what the frame showed, so the next frame does not snap back
    Rig.Tick();
    REQUIRE(Rig.Sim().Policy.Frozen);
    REQUIRE(SameBits(Rig.Sim().View.Yaw, BeforePress));
    Rig.CheckSimHasAll();
    Rig.Frame();
    Rig.CheckFrame();
    REQUIRE(SameBits(Rig.View().Yaw, BeforePress));

    // Input during the freeze moves neither side
    Rig.Move(30, 30);
    Rig.Frame();
    Rig.CheckFrame();
    REQUIRE(SameBits(Rig.View().Yaw, BeforePress));

    // Unfreeze inside the same pending window: the 30 counts before the press stay dropped,
    // the 9 after it turn the view, on both sides
    Rig.Press(Keys::F);
    Rig.Move(9, 0);
    Rig.Frame();
    Rig.CheckFrame();
    REQUIRE(Rig.View().Yaw == Catch::Approx(-0.24f));
    const float Shown = Rig.View().Yaw;
    Rig.Tick();
    REQUIRE_FALSE(Rig.Sim().Policy.Frozen);
    REQUIRE(SameBits(Rig.Sim().View.Yaw, Shown));
    Rig.CheckSimHasAll();
}

TEST_CASE("Late look: a LookScale change takes effect at its place in the input", "[look]")
{
    LookRig Rig(MakeCamera(0.01f));
    Rig.Tick();

    // 10 counts at 0.01, then Equal, then 10 counts at 0.02, in one pending window
    Rig.Move(10, 0);
    Rig.Press(Keys::Equal);
    Rig.Move(10, 0);
    Rig.Frame();
    Rig.CheckFrame();
    REQUIRE(Rig.View().Yaw == Catch::Approx(-0.3f));
    const float Shown = Rig.View().Yaw;
    Rig.Tick();
    REQUIRE(Rig.Sim().Policy.LookScale == 0.02f);
    REQUIRE(SameBits(Rig.Sim().View.Yaw, Shown));

    // Minus in the middle of the next window: 5 counts at 0.02, 5 at 0.01
    Rig.Move(5, 0);
    Rig.Press(Keys::Minus);
    Rig.Move(5, 0);
    Rig.Frame();
    Rig.CheckFrame();
    REQUIRE(Rig.View().Yaw == Catch::Approx(-0.45f));
    Rig.Tick();
    REQUIRE(Rig.Sim().Policy.LookScale == 0.01f);
    Rig.CheckSimHasAll();
    Rig.Frame();
    REQUIRE(SameBits(Rig.View(), Rig.Sim().View));
}

namespace
{
    /** A fixed, seeded stream of moves and presses of F, Equal and Minus, with flicks past the pitch clamp. */
    std::vector<StreamEvent> MakeEdgeScript()
    {
        std::vector<StreamEvent> Script;
        uint32 State = 12345;
        const auto Next = [&State](const uint32 t_Range)
        {
            State = State * 1664525u + 1013904223u;
            return (State >> 8) % t_Range;
        };
        for (int i = 0; i < 90; ++i)
        {
            const uint32 Roll = Next(10);
            if (Roll == 0)
            {
                Script.push_back(StreamEvent{.Mouse = false, .Key = Keys::F, .Down = true});
            }
            else if (Roll == 1)
            {
                Script.push_back(StreamEvent{.Mouse = false, .Key = Next(2) == 0 ? Keys::Equal : Keys::Minus});
                Script.back().Down = true;
            }
            else
            {
                const int32 Dx = static_cast<int32>(Next(41)) - 20;
                const int32 Dy = (i % 17 == 3) ? -400 : (i % 17 == 4 ? 380 : static_cast<int32>(Next(61)) - 30);
                Script.push_back(StreamEvent{.Mouse = true, .Dx = Dx, .Dy = Dy});
            }
        }
        return Script;
    }

    /**
     * Pushes the script (a press is a KeyDown and a KeyUp), runs a tick after the events
     * that t_TickAfter picks, and t_Frames frames after each event. Checks at every frame
     * that the view is the whole stream so far run from the start camera: it depends on
     * the input only, so it cannot jump when a tick consumes the input. Returns the camera
     * after the last tick.
     */
    LookCamera RunEdgeSplit(const std::vector<StreamEvent>& t_Script,
                            const std::function<bool(size_t)>& t_TickAfter,
                            const int t_Frames)
    {
        LookRig Rig(MakeCamera(0.004f, 1.3f));
        Rig.Tick();
        for (size_t i = 0; i < t_Script.size(); ++i)
        {
            const StreamEvent& Event = t_Script[i];
            if (Event.Mouse)
            {
                Rig.Move(Event.Dx, Event.Dy);
            }
            else
            {
                Rig.Press(Event.Key);
            }
            if (t_TickAfter(i))
            {
                Rig.Tick();
            }
            for (int f = 0; f < t_Frames; ++f)
            {
                Rig.Frame();
                Rig.CheckFrame();
                REQUIRE(SameBits(Rig.View(), Reference(Rig.StartCamera, Rig.Stream).View));
            }
        }
        Rig.Tick();
        Rig.CheckSimHasAll();
        return Rig.Sim();
    }
} // namespace

TEST_CASE("Late look: key edges inside ticks give the same camera for every split", "[look]")
{
    const std::vector<StreamEvent> Script = MakeEdgeScript();
    size_t Presses = 0;
    for (const StreamEvent& Event : Script)
    {
        Presses += Event.Mouse ? 0 : 1;
    }
    REQUIRE(Presses >= 10);

    std::vector<std::function<bool(size_t)>> Splits;
    for (const size_t Every : {1, 2, 3, 5, 8, 1000})
    {
        Splits.emplace_back([Every](const size_t i) { return (i + 1) % Every == 0; });
    }
    // A tick right after each press, and right before each press
    Splits.emplace_back([&Script](const size_t i) { return !Script[i].Mouse; });
    Splits.emplace_back([&Script](const size_t i) { return i + 1 < Script.size() && !Script[i + 1].Mouse; });
    // Seeded uneven splits, including hitches (a tick that consumes nothing comes from two ticks in a row)
    for (const uint32 Seed : {7u, 99u})
    {
        auto State = std::make_shared<uint32>(Seed);
        Splits.emplace_back(
            [State](size_t)
            {
                *State = *State * 22695477u + 1u;
                return (*State >> 16) % 3 == 0;
            });
    }

    const LookCamera Expected = RunEdgeSplit(Script, Splits[0], 1);
    UNSCOPED_INFO("final yaw " << Expected.View.Yaw << " pitch " << Expected.View.Pitch << " scale "
                               << Expected.Policy.LookScale << " frozen " << Expected.Policy.Frozen);
    for (size_t s = 0; s < Splits.size(); ++s)
    {
        for (const int Frames : {0, 1, 2})
        {
            INFO("split " << s << ", " << Frames << " frames per event");
            REQUIRE(SameBits(RunEdgeSplit(Script, Splits[s], Frames), Expected));
        }
    }
}

namespace
{
    /** The tick on which the test cutscenes freeze the camera. Tick 1 is the first. */
    constexpr uint64 CutsceneTick = 3;

    /**
     * A cutscene: freezes every LookCamera on CutsceneTick, for a reason that is not input.
     * It runs after Sandbox.LookCamera, so the freeze takes effect at the end of the tick,
     * after the tick's input was used.
     */
    class CutsceneAfterLookSystem final
        : public System<CutsceneAfterLookSystem, Access<Read<SimTime>, Write<LookCamera>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Test.CutsceneAfterLook", .In = Order::Simulation, .After = SystemOf<Sandbox::LookCameraSystem>};

        void FixedUpdate(Context& ctx) const
        {
            if (ctx.Singleton<const SimTime>().Tick == CutsceneTick)
            {
                for (auto [entity, camera] : ctx.Query<LookCamera>())
                {
                    camera.Policy.Frozen = true;
                }
            }
        }
    };

    /** A cutscene that does not say where it runs against Sandbox.LookCamera. */
    class CutsceneUnorderedSystem final : public System<CutsceneUnorderedSystem, Access<Write<LookCamera>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.CutsceneUnordered", .In = Order::Simulation};

        void FixedUpdate(Context&) const {}
    };

    /** The same cutscene, before Sandbox.LookCamera: the freeze takes effect at the start of the tick. */
    class CutsceneBeforeLookSystem final
        : public System<CutsceneBeforeLookSystem, Access<Read<SimTime>, Write<LookCamera>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Test.CutsceneBeforeLook", .In = Order::Simulation, .Before = SystemOf<Sandbox::LookCameraSystem>};

        void FixedUpdate(Context& ctx) const
        {
            if (ctx.Singleton<const SimTime>().Tick == CutsceneTick)
            {
                for (auto [entity, camera] : ctx.Query<LookCamera>())
                {
                    camera.Policy.Frozen = true;
                }
            }
        }
    };
} // namespace

TEST_CASE("Late look: a freeze that does not come from input is shown from the next sealed state", "[look]")
{
    SECTION("At the end of the tick (after Sandbox.LookCamera): no jump, the frame obeys at once")
    {
        LookRig Rig(MakeCamera(0.01f), [](SystemRegistry& t_Systems) { t_Systems.Add<CutsceneAfterLookSystem>(); });
        Rig.Tick();
        Rig.Tick();
        Rig.Move(10, 0);
        Rig.Frame();
        Rig.CheckFrame();
        // The frame side cannot know about the cutscene: it shows the move
        const float Shown = Rig.View().Yaw;
        REQUIRE(Shown == Catch::Approx(-0.1f));

        Rig.Tick();
        REQUIRE(Rig.Storage.Singleton<SimTime>().Tick == CutsceneTick);
        REQUIRE(Rig.Sim().Policy.Frozen);
        REQUIRE(SameBits(Rig.Sim().View.Yaw, Shown));
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(SameBits(Rig.View().Yaw, Shown));

        // From the frozen sealed state on, pending input is not shown
        Rig.Move(20, 0);
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(SameBits(Rig.View().Yaw, Shown));
        Rig.Tick();
        REQUIRE(SameBits(Rig.Sim().View.Yaw, Shown));
    }

    SECTION("At the start of the tick (before Sandbox.LookCamera): the input that was shown is dropped")
    {
        LookRig Rig(MakeCamera(0.01f), [](SystemRegistry& t_Systems) { t_Systems.Add<CutsceneBeforeLookSystem>(); });
        Rig.Tick();
        Rig.Tick();
        Rig.Move(10, 0);
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.View().Yaw == Catch::Approx(-0.1f));

        // The cutscene freezes the camera before the look runs, so the tick drops the 10
        // counts that the frame already showed: a snap back of 0.1 rad
        Rig.Tick();
        REQUIRE(Rig.Sim().Policy.Frozen);
        REQUIRE(Rig.Sim().View.Yaw == 0.0f);
        Rig.Frame();
        Rig.CheckFrame();
        REQUIRE(Rig.View().Yaw == 0.0f);
    }
}

TEST_CASE("Late poll: input that comes in while the tick runs is pending on that frame", "[look]")
{
    // The engine loop with one tick per frame: poll, tick, (late poll), frame systems. Two
    // moves come in each frame: one before the poll at the start of the frame, one while the
    // tick runs. Without the late poll, a frame that ran a tick has nothing pending.
    const auto Run = [](const bool t_LatePoll, uint32& t_PendingOnTickFrames)
    {
        LookRig Rig(MakeCamera(0.01f));
        std::vector<LookView> Views;
        t_PendingOnTickFrames = 0;
        for (int Frame = 0; Frame < 12; ++Frame)
        {
            Rig.QueueMove(1 + Frame, 0);
            Rig.Poll();
            Rig.Tick();
            Views.push_back(Rig.Sim().View);
            Rig.QueueMove(0, 2 + Frame);
            if (t_LatePoll)
            {
                Rig.Poll();
            }
            Rig.Frame();
            t_PendingOnTickFrames += Rig.PendingSamples();
            if (t_LatePoll)
            {
                Rig.CheckFrame();
            }
        }
        Rig.Poll();
        Rig.Tick();
        Views.push_back(Rig.Sim().View);
        Rig.CheckSimHasAll();
        return Views;
    };

    uint32 PendingBefore = 0;
    uint32 PendingAfter = 0;
    const std::vector<LookView> Before = Run(false, PendingBefore);
    const std::vector<LookView> After = Run(true, PendingAfter);
    UNSCOPED_INFO("pending samples on 12 frames with a tick: " << PendingBefore << " without the late poll, "
                                                               << PendingAfter << " with it");
    REQUIRE(PendingBefore == 0);
    REQUIRE(PendingAfter == 12);

    // The ticks get the same messages, each one once and in order, so the simulation is the same
    REQUIRE(Before.size() == After.size());
    for (size_t i = 0; i < Before.size(); ++i)
    {
        REQUIRE(SameBits(Before[i], After[i]));
    }
}

TEST_CASE("Late look: a flick past the pitch clamp and back", "[look]")
{
    // Pitch 1.4, clamp at 1.5, 0.01 rad per count. Up 30 counts (+0.3, clamped to 1.5), then down 30 (-0.3).
    SECTION("The frame side shows what the tick makes, whatever the split")
    {
        LookRig Rig(MakeCamera(0.01f, 1.4f));
        Rig.Tick();
        Rig.Move(0, -30);
        Rig.Frame();
        REQUIRE(Rig.View().Pitch == 1.5f);
        Rig.Move(0, 30);
        Rig.Frame();
        const float Shown = Rig.View().Pitch;
        REQUIRE(Shown == Catch::Approx(1.2f));
        Rig.Tick();
        REQUIRE(SameBits(Rig.Sim().View.Pitch, Shown));

        // Split over two ticks: the same result
        LookRig Split(MakeCamera(0.01f, 1.4f));
        Split.Tick();
        Split.Move(0, -30);
        Split.Tick();
        Split.Move(0, 30);
        Split.Frame();
        REQUIRE(SameBits(Split.View().Pitch, Shown));
        Split.Tick();
        REQUIRE(SameBits(Split.Sim().View.Pitch, Shown));
    }
}

TEST_CASE("Late look: the cut and the sim view must come from the same tick", "[look]")
{
    // The frame side reads the cut from the TickStamp it shows (RealFrameTime::Latest): the
    // ledger's newest stamp with one storage, the packet's with a mirror. This shows what a
    // torn pair would do, with a stamp from another tick than the state: the frame then
    // shows some input twice, or not at all.
    LookRig Rig(MakeCamera(0.01f));
    Rig.Move(10, 0);
    Rig.Tick();
    const TickStamp OldStamp = Rig.Runner->LatestTickStamp();
    const LookView OldView = Rig.Sim().View;
    Rig.Move(5, 0);
    Rig.Tick();
    REQUIRE(Rig.Sim().View.Yaw == Catch::Approx(-0.15f));

    SECTION("A newer view with an older stamp counts the 5 counts twice")
    {
        Rig.Runner->RunFrameAt(0, std::chrono::milliseconds(4), WorldRole::Play, OldStamp);
        REQUIRE(Rig.View().Yaw == Catch::Approx(-0.20f));
    }

    SECTION("An older view with a newer stamp loses them for that frame")
    {
        Rig.SimMut().View = OldView;
        Rig.Frame();
        REQUIRE(Rig.View().Yaw == Catch::Approx(-0.10f));
    }
}

TEST_CASE("Late look: the view system needs no order, because the runner writes PendingInput first", "[look][schedule]")
{
    SystemRegistry Systems;
    ModuleRegistration Modules(Systems);
    RegisterEngine(Modules);
    Systems.Add<Sandbox::LookCameraSystem>();
    Systems.Add<Sandbox::LookCameraViewSystem>();

    STATIC_REQUIRE(Sandbox::LookCameraViewSystem::Info.After.Size() == 0);
    STATIC_REQUIRE(Sandbox::LookCameraViewSystem::Info.Before.Size() == 0);
    STATIC_REQUIRE(RateOf(Sandbox::LookCameraViewSystem::Info.In) == UpdateRate::Variable);

    const ScheduleResult Result = Systems.BuildSchedule();
    REQUIRE(Result.ErrorReport() == "");
    const Schedule& Built = Result.Value;

    std::vector<String> Frame;
    for (const uint32 i : Built.RunOrder(UpdateRate::Variable))
    {
        Frame.emplace_back(Built.Systems[i]->Info.Name);
    }
    REQUIRE(Frame == std::vector<String>{"Sandbox.LookCameraView"});

    // No system writes PendingInput or InputState: no edge is about them
    for (const ScheduleEdge& Edge : Built.Edges)
    {
        const String Type = Edge.TypeName != nullptr ? String(Edge.TypeName) : String();
        REQUIRE(Type != "PendingInput");
        REQUIRE(Type != "InputState");
    }

    const String Dump = Built.Dump();
    INFO(Dump);
    REQUIRE(Dump.find("Sandbox.LookCameraView       [Engine.Presentation]") != String::npos);
    REQUIRE(Dump.find("  the runner writes first: RealFrameTime, PendingInput\n") != String::npos);
    // One fixed system now: input and policy in one walk (LookUtil::Run), so no .After is needed
    REQUIRE(Dump.find("Sandbox.LookCamera           [Engine.Simulation]") != String::npos);
    REQUIRE(Dump.find("resim runs: Sandbox.LookCamera") != String::npos);
}

TEST_CASE("Late look: a second writer of LookCamera must be ordered, and the fix names the class", "[look][schedule]")
{
    SystemRegistry Systems;
    Systems.Add<Sandbox::LookCameraSystem>();
    Systems.Add<CutsceneUnorderedSystem>();
    const ScheduleResult Result = Systems.BuildSchedule();
    REQUIRE(Result.Errors.size() == 1);
    REQUIRE(Result.Errors[0].Kind == ScheduleErrorKind::WriteConflict);
    INFO(Result.Errors[0].Message);
    REQUIRE(
        Result.Errors[0].Message.find(
            "Fix: add .After = Fling::SystemOf<Sandbox::LookCameraSystem> to Test.CutsceneUnordered's SystemInfo") !=
        String::npos);
}

namespace
{
    /** A slider in the look camera's world: X = 2 * tick, a pure function of the tick. */
    struct LookSlide
    {
        static constexpr ComponentInfo Component{.Name = "LookTest.LookSlide"};
        float X = 0.0f;
    };

    class LookSlideSystem final : public System<LookSlideSystem, Access<Read<SimTime>, Write<LookSlide>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "LookTest.Slide", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float Tick = static_cast<float>(ctx.Singleton<const SimTime>().Tick);
            for (auto [entity, slide] : ctx.Query<LookSlide>())
            {
                slide.X = 2.0f * Tick;
            }
        }
    };

    /** Where the last frame drew the slider. */
    float& DrawnSlideX()
    {
        static float s_X = 0.0f;
        return s_X;
    }

    /** Draws the slider as the renderer does: BlendFrom toward BlendTo by BlendAlpha. */
    class LookSlideDrawSystem final
        : public System<LookSlideDrawSystem, Access<Read<RealFrameTime>, Read<LookSlide>, ReadBlend<LookSlide>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "LookTest.SlideDraw", .In = Order::Presentation};

        void Update(Context& ctx) const
        {
            const float Alpha = ctx.Singleton<const RealFrameTime>().BlendAlpha;
            for (auto [entity, slide] : ctx.Query<const LookSlide>())
            {
                DrawnSlideX() =
                    Interpolate::Lerp(ctx.BlendFrom<LookSlide>(entity).X, ctx.BlendTo<LookSlide>(entity).X, Alpha);
            }
        }
    };
} // namespace

TEST_CASE("Late look and blend in one frame: the object is drawn between the two latest ticks, the camera has "
          "the input after the latest cut",
          "[look][interp]")
{
    LookRig Rig(MakeCamera(0.5f),
                [](SystemRegistry& t_Systems)
                {
                    t_Systems.Add<LookSlideSystem>();
                    t_Systems.Add<LookSlideDrawSystem>();
                });
    const Entity Slider = Rig.Storage.Create(LookSlide{});
    const int64 L = SystemRunnerConfig{}.FixedStep().count();

    // The frame side as in Direct play: a DirectLink that publishes after each tick, and the
    // blend of the latest two ticks it shows, from the state play starts with (tick 0)
    DirectLink Link(Rig.Storage, *Rig.Runner, {Detail::ComponentIdOf<LookSlide>()});
    FrameBlend Blend;
    Blend.SetTypes({Detail::ComponentIdOf<LookSlide>()});
    Link.Publish(Rig.Storage, *Rig.Runner);
    Rig.Runner->SetTickOutput(
        [&](uint64)
        {
            Link.Publish(Rig.Storage, *Rig.Runner);
            return true;
        });
    Rig.Runner->StartTimeline(0);
    const auto MoveAt = [&](const int64 t_Ns, const int32 t_Dx, const int32 t_Dy)
    {
        Rig.NowNs = t_Ns - 1000;
        Rig.Move(t_Dx, t_Dy);
    };
    const auto FrameAt = [&](const int64 t_Ns)
    {
        Rig.Clock.Clock().Set(t_Ns);
        Rig.Runner->RunFixedAt(t_Ns, WorldRole::Play);
    };
    const auto Draw = [&](const int64 t_Ns)
    {
        const ShownTicks Shown = Link.ShowLatest();
        Blend.Show(Shown);
        Rig.Runner->RunFrameAt(t_Ns, std::chrono::milliseconds(4), WorldRole::Play, Shown.Latest, Blend.Advance(t_Ns));
    };

    // The first frame of play, half a tick before tick 1 is due, draws tick 0
    const int64 F0 = L / 2;
    Draw(F0);
    CHECK(DrawnSlideX() == 0.0f);

    // Tick 1 takes the first move. The frame that runs it gets a second move after the tick's
    // cut (its late poll), then draws.
    MoveAt(5'000'000, 3, 1);
    const int64 F1 = L + 1'000'000;
    FrameAt(F1);
    REQUIRE(Rig.Runner->TickCount() == 1);
    REQUIRE(Rig.Sim().View.Yaw != 0.0f);
    MoveAt(F1 + 100'000, 5, 2);
    Draw(F1);
    // The drawn point goes on from tick 0 by the frame time since the first frame
    const RealFrameTime& Time = Rig.Storage.Singleton<RealFrameTime>();
    const auto TicksSince = [&](const int64 t_Ns)
    {
        return static_cast<double>(t_Ns - F0) / static_cast<double>(L);
    };
    CHECK(Time.BlendFrom.Tick == 0);
    CHECK(Time.BlendTo.Tick == 1);
    CHECK(Time.BlendAlpha == Catch::Approx(TicksSince(F1)));
    CHECK(DrawnSlideX() == Interpolate::Lerp(0.0f, 2.0f, Time.BlendAlpha));
    REQUIRE(Rig.PendingSamples() == 1);
    CHECK(SameBits(Rig.View(), Reference(Rig.Sim(), std::vector<StreamEvent>{{.Dx = 5, .Dy = 2}}).View));

    // A quarter tick later, no new tick, one more move: the object is still between tick 0 and
    // tick 1 while the camera already has both moves after tick 1's cut
    const int64 F2 = F1 + L / 4;
    MoveAt(F2, 7, -1);
    FrameAt(F2);
    REQUIRE(Rig.Runner->TickCount() == 1);
    Draw(F2);
    const float Alpha = Time.BlendAlpha;
    CHECK(Alpha == Catch::Approx(TicksSince(F2)));
    CHECK(DrawnSlideX() == Interpolate::Lerp(0.0f, 2.0f, Alpha));
    CHECK(DrawnSlideX() > 0.0f);
    CHECK(DrawnSlideX() < 2.0f);
    CHECK(Rig.Storage.Get<LookSlide>(Slider).X == 2.0f);
    REQUIRE(Rig.PendingSamples() == 2);
    const std::vector<StreamEvent> AfterCut{{.Dx = 5, .Dy = 2}, {.Dx = 7, .Dy = -1}};
    CHECK(SameBits(Rig.View(), Reference(Rig.Sim(), AfterCut).View));
    CHECK_FALSE(SameBits(Rig.View(), Rig.Sim().View));

    // Tick 2 takes both moves: the object goes on between tick 1 and tick 2, and nothing is pending
    const int64 F3 = 2 * L + 500'000;
    FrameAt(F3);
    REQUIRE(Rig.Runner->TickCount() == 2);
    Draw(F3);
    CHECK(Time.BlendFrom.Tick == 1);
    CHECK(Time.BlendTo.Tick == 2);
    CHECK(Time.BlendAlpha == Catch::Approx(TicksSince(F3) - 1.0));
    CHECK(DrawnSlideX() == Interpolate::Lerp(2.0f, 4.0f, Time.BlendAlpha));
    CHECK(Rig.PendingSamples() == 0);
    CHECK(SameBits(Rig.View(), Rig.Sim().View));
    Rig.Runner->SetTickOutput({});
}
