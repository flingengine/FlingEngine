// Tests of tick identity and timing: the runner's timeline and ledger (TickStamp per
// tick), input assigned to ticks by time, replay by recorded cuts, the state hash, and
// the presentation time in RealFrameTime.
//
// Everything runs on a virtual platform clock (PlatformTime::ScopedVirtualClock), so no
// test depends on wall time. Input goes through the real queue: a virtual mouse and
// keyboard, the engine's TickInputProvider for the ticks, PendingInputReader for the
// frames, and the test camera systems (TestSystems/TestCamera.h) as the simulation state.

#include "Components/CameraView.h"
#include "Components/EntityGuid.h"
#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/SimTime.h"
#include "Ecs/StateHash.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Ecs/TypeRegistry.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputPlayback.h"
#include "Input/InputRecorder.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "PlatformTime.h"
#include "TestSystems/TestCamera.h"

#include <catch2/catch_all.hpp>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <functional>
#include <memory>
#include <vector>

using namespace Fling;
using namespace std::chrono_literals;
using TestLook::TestCamera;

namespace
{
    constexpr int64 Ms = 1'000'000;

    /** One input event of a script: a mouse move or a key edge, at a time on the platform clock. */
    struct ScriptEvent
    {
        int64 TimeNs = 0;
        bool Mouse = true;
        int32 Dx = 0;
        int32 Dy = 0;
        InputKey Key;
        bool Down = false;
    };

    ScriptEvent MoveAt(const int64 t_TimeNs, const int32 t_Dx, const int32 t_Dy = 0)
    {
        return ScriptEvent{.TimeNs = t_TimeNs, .Mouse = true, .Dx = t_Dx, .Dy = t_Dy};
    }

    ScriptEvent KeyAt(const int64 t_TimeNs, const InputKey t_Key, const bool t_Down)
    {
        return ScriptEvent{.TimeNs = t_TimeNs, .Mouse = false, .Key = t_Key, .Down = t_Down};
    }

    /** A small deterministic generator, so the script is the same on every platform. */
    struct Lcg
    {
        uint64 State = 0;

        uint32 Next()
        {
            State = State * 6364136223846793005ull + 1442695040888963407ull;
            return static_cast<uint32>(State >> 33);
        }

        int32 Range(const int32 t_Min, const int32 t_Max)
        {
            return t_Min + static_cast<int32>(Next() % static_cast<uint32>(t_Max - t_Min + 1));
        }
    };

    /**
     * About one second of look input: mouse moves (some flick past the pitch clamp) and
     * presses of F (freeze), Equal and Minus (look scale), at seeded times that never
     * repeat.
     */
    std::vector<ScriptEvent> MakeScript(const uint64 t_Seed, const int t_Count)
    {
        Lcg Random{t_Seed};
        std::vector<ScriptEvent> Script;
        int64 Time = 1 * Ms;
        const InputKey PolicyKeys[] = {Keys::F, Keys::Equal, Keys::Minus};
        for (int i = 0; i < t_Count; ++i)
        {
            Time += Random.Range(1, 9) * Ms + Random.Range(1, 999) * 1000;
            if (Random.Range(0, 9) == 0)
            {
                const InputKey Key = PolicyKeys[Random.Range(0, 2)];
                Script.push_back(KeyAt(Time, Key, true));
                Time += Random.Range(1, 30) * 100'000;
                Script.push_back(KeyAt(Time, Key, false));
            }
            else
            {
                const bool Flick = Random.Range(0, 7) == 0;
                Script.push_back(
                    MoveAt(Time, Random.Range(-40, 40), Flick ? Random.Range(-900, 900) : Random.Range(-30, 30)));
            }
        }
        return Script;
    }

    /** The columns of a TickRecord that must not depend on the frame rate: the stamp and the input counts. */
    struct Row
    {
        TickStamp Stamp;
        uint32 MouseSamples = 0;
        uint32 KeyEvents = 0;

        bool operator==(const Row&) const = default;
    };

    /** The rows that a replay must reproduce: no sequences, which depend on the input devices. */
    struct InputRow
    {
        uint64 Tick = 0;
        uint32 MouseSamples = 0;
        uint32 KeyEvents = 0;
        uint64 StateHash = 0;

        bool operator==(const InputRow&) const = default;
    };

    std::vector<InputRow> InputRows(const std::vector<Row>& t_Rows)
    {
        std::vector<InputRow> Out;
        for (const Row& Each : t_Rows)
        {
            Out.push_back(InputRow{Each.Stamp.Tick, Each.MouseSamples, Each.KeyEvents, Each.Stamp.StateHash});
        }
        return Out;
    }

    /**
     * The engine loop on a virtual clock: a virtual mouse and keyboard, the runner's input,
     * the pending input and the test camera systems, one TestCamera, a state hasher over
     * the registered types, and the engine's TickInputProvider (time rule, or the
     * playback's recorded cut, noted to the recorder). Frame(t) delivers the script
     * events up to t (an input thread stamps them with their own times), runs the ticks
     * that are due at t, then the frame systems.
     */
    struct TimeRig
    {
        PlatformTime::ScopedVirtualClock Clock{0};

        std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
        std::shared_ptr<VirtualInputDeviceInterface> Virtual;
        InputDeviceId Mouse;
        InputDeviceId Keyboard;

        EntityStorage Storage;
        SystemRegistry Systems;
        TypeRegistry Types;
        std::unique_ptr<SystemRunner> Runner;
        Entity Camera;

        /** Idle until a test starts them. */
        std::unique_ptr<InputPlayback> Playback = std::make_unique<InputPlayback>(Handler);
        std::unique_ptr<InputRecorder> Recorder = std::make_unique<InputRecorder>(Handler);

        std::vector<ScriptEvent> Script;
        size_t NextEvent = 0;

        /** Sequence of each script event once it is in the queue (0 before). */
        std::vector<uint64> Sequences;

        int64 LastFrameNs = 0;

        explicit TimeRig(const SystemRunnerConfig& t_Config = {}, const std::vector<ScriptEvent>& t_Script = {})
            : Script(t_Script), Sequences(t_Script.size(), 0)
        {
            const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
            Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);

            InputDeviceInfo MouseInfo;
            MouseInfo.Type = InputDeviceType::Mouse;
            MouseInfo.HardwarePath = "virtual:mouse";
            MouseInfo.Capabilities = InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::RelativeMotion;
            Mouse = Virtual->CreateDevice(MouseInfo);
            InputDeviceInfo KeyboardInfo;
            KeyboardInfo.Type = InputDeviceType::Keyboard;
            KeyboardInfo.HardwarePath = "virtual:keyboard";
            KeyboardInfo.Capabilities = InputDeviceCapabilities::Keys;
            Keyboard = Virtual->CreateDevice(KeyboardInfo);
            Handler->PollInterfaces(0);
            Virtual->SetOnSent([this](const uint64 t_Tag, const uint64 t_Sequence)
                               { Sequences[t_Tag - 1] = t_Sequence; });

            Systems.Add<TestLook::CameraSystem>();
            Systems.Add<TestLook::CameraViewSystem>();
            ModuleRegistration Modules(Systems);
            TestLook::RegisterCameraType(Modules);
            Types = TypeRegistry::Build(Modules);
            REQUIRE(Types.Ok());

            ScheduleResult Schedule = Systems.BuildSchedule();
            REQUIRE(Schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(Schedule.Value), Storage, t_Config);

            auto Inputs = std::make_shared<TickInputProvider>(Handler, Playback.get(), Recorder.get());
            Runner->SetTickInputSource([Inputs](TickInput& t_Out, const TickWindow& t_Window)
                                       { Inputs->Build(t_Out, t_Window); });
            auto Pending = std::make_shared<PendingInputReader>(Handler);
            Runner->SetPendingInputSource([Pending](const uint64 t_After, InputSlice& t_Out)
                                          { Pending->Read(t_After, t_Out); });
            Runner->SetStateHasher([this](const EntityStorage& t_Storage)
                                   { return HashSimulationState(t_Storage, Types); });
            Runner->StartTimeline(0);
            Runner->Create();

            Camera = Storage.Create(TestCamera{});
        }

        /** Queue the script events with a time at or before t_NowNs, each with its own time. */
        void Deliver(const int64 t_NowNs)
        {
            while (NextEvent < Script.size() && Script[NextEvent].TimeNs <= t_NowNs)
            {
                const ScriptEvent& Event = Script[NextEvent];
                InputMessage Message;
                Message.TimeNs = Event.TimeNs;
                if (Event.Mouse)
                {
                    Message.Device = Mouse;
                    Message.Type = InputMessageType::MouseMove;
                    Message.DeltaX = Event.Dx;
                    Message.DeltaY = Event.Dy;
                }
                else
                {
                    Message.Device = Keyboard;
                    Message.Type = Event.Down ? InputMessageType::KeyDown : InputMessageType::KeyUp;
                    Message.Key = Event.Key;
                }
                ++NextEvent;
                Virtual->Queue(Message, NextEvent);
            }
        }

        /**
         * One frame at t_NowNs: playback and input poll, the ticks that are due, the
         * recorder, the frame systems. t_PlaybackSeconds moves the playback and recorder
         * clocks (by default the frame's real delta).
         */
        uint32 Frame(const int64 t_NowNs, const double t_PlaybackSeconds = -1.0)
        {
            const double Delta =
                t_PlaybackSeconds >= 0.0 ? t_PlaybackSeconds : static_cast<double>(t_NowNs - LastFrameNs) / 1e9;
            Clock.Clock().Set(t_NowNs);
            Playback->Update(static_cast<float>(Delta));
            Deliver(t_NowNs);
            Handler->PollInterfaces(t_NowNs);
            Recorder->Update(static_cast<float>(Delta));
            const uint32 Ticks = Runner->RunFixedAt(t_NowNs, WorldRole::Play);
            Runner->RunFrameAt(t_NowNs, std::chrono::nanoseconds(t_NowNs - LastFrameNs), WorldRole::Play);
            LastFrameNs = t_NowNs;
            return Ticks;
        }

        std::vector<Row> Rows() const
        {
            std::vector<Row> Out;
            const TickLedger& Ledger = Runner->GetLedger();
            for (size_t i = 0; i < Ledger.Size(); ++i)
            {
                const TickRecord& R = Ledger.At(i);
                Out.push_back(Row{R.Stamp, R.MouseSamples, R.KeyEvents});
            }
            return Out;
        }

        const TestCamera& Sim() const { return *Storage.TryGet<TestCamera>(Camera); }

        const PendingInput& Pending() const { return Storage.Singleton<PendingInput>(); }

        /** The stamp of the tick the last frame showed. */
        const TickStamp& Shown() const { return Storage.Singleton<RealFrameTime>().Latest; }

        uint64 Cut() const { return Shown().Cut.LastSequence; }

        /** The delivered script mouse events after the cut, as the frame side must see them. */
        size_t PendingMouseExpected() const
        {
            // Before the first tick there is no cut, and nothing is pending (SystemRunner::RunFrameAt)
            if (Shown().Tick == 0)
            {
                return 0;
            }
            size_t Count = 0;
            for (size_t i = 0; i < NextEvent; ++i)
            {
                if (Script[i].Mouse && Sequences[i] > Cut())
                {
                    ++Count;
                }
            }
            return Count;
        }
    };

    /** Run frames at the times that t_Next gives until t_Ticks ticks ran. */
    std::vector<Row> RunUntil(TimeRig& t_Rig, const uint64 t_Ticks, const std::function<int64(uint64)>& t_FrameTime)
    {
        for (uint64 Frame = 1; t_Rig.Runner->TickCount() < t_Ticks; ++Frame)
        {
            t_Rig.Frame(t_FrameTime(Frame));
            // The frame side always shows exactly the input after the cut
            REQUIRE(t_Rig.Pending().Cut.AfterSequence == t_Rig.Cut());
            REQUIRE(t_Rig.Pending().MouseSamples.size() == t_Rig.PendingMouseExpected());
        }
        std::vector<Row> Rows = t_Rig.Rows();
        Rows.resize(t_Ticks);
        return Rows;
    }
} // namespace

TEST_CASE("Tick time: a hitch spreads input over its ticks by time", "[time]")
{
    // Tick k covers [(k - 1) * 10 ms, k * 10 ms)
    const std::vector<ScriptEvent> Script{MoveAt(2 * Ms, 1),
                                          MoveAt(5 * Ms, 2),
                                          MoveAt(12 * Ms, 3),
                                          MoveAt(25 * Ms, 4),
                                          KeyAt(26 * Ms, Keys::Equal, true),
                                          KeyAt(27 * Ms, Keys::Equal, false),
                                          MoveAt(30 * Ms - 1, 5),
                                          MoveAt(30 * Ms, 6),
                                          MoveAt(31 * Ms, 7),
                                          MoveAt(40 * Ms, 8),
                                          MoveAt(45 * Ms, 9)};
    TimeRig Rig(SystemRunnerConfig{.SimHz = 100}, Script);

    REQUIRE(Rig.Frame(9 * Ms) == 0);
    REQUIRE(Rig.Pending().MouseSamples.empty()); // no cut before the first tick
    REQUIRE(Rig.Frame(10 * Ms) == 1);

    // The hitch: a frame at 40 ms runs ticks 2, 3 and 4
    REQUIRE(Rig.Frame(40 * Ms) == 3);
    std::vector<Row> Rows = Rig.Rows();
    REQUIRE(Rows.size() == 4);
    const auto Samples = [&](const size_t t_Tick)
    {
        return Rows[t_Tick - 1].MouseSamples;
    };
    REQUIRE(Samples(1) == 2); // 2, 5
    REQUIRE(Samples(2) == 1); // 12
    REQUIRE(Samples(3) == 2); // 25, 30 ms - 1 ns
    REQUIRE(Rows[2].KeyEvents == 2);
    REQUIRE(Samples(4) == 2); // 30 (on the boundary: the later tick), 31
    for (size_t i = 0; i < Rows.size(); ++i)
    {
        REQUIRE(Rows[i].Stamp.Tick == i + 1);
        REQUIRE(Rows[i].Stamp.Window.StartNs == static_cast<int64>(i) * 10 * Ms);
        REQUIRE(Rows[i].Stamp.Window.EndNs == static_cast<int64>(i + 1) * 10 * Ms);
        if (i > 0)
        {
            REQUIRE(Rows[i].Stamp.Cut.AfterSequence == Rows[i - 1].Stamp.Cut.LastSequence);
        }
    }
    // The cut is exact: tick 4 ends at the message at 31 ms, and the one at 40 ms is pending
    REQUIRE(Rows[3].Stamp.Cut.LastSequence == Rig.Sequences[8]);
    REQUIRE(Rig.Pending().MouseSamples.size() == 1);
    REQUIRE(Rig.Pending().MouseSamples[0].Sequence == Rig.Sequences[9]);
    REQUIRE(Rig.Pending().Cut.AfterSequence == Rows[3].Stamp.Cut.LastSequence);

    // The Equal press in tick 3 doubled the scale after the move at 25 ms, before the one at 30 ms - 1
    const float Expected = -(1 + 2 + 3 + 4) * 0.01f - 5 * 0.02f - (6 + 7) * 0.02f;
    REQUIRE(Rig.Sim().Yaw == Catch::Approx(Expected));

    // A late message: 45 ms is in tick 5's window [40, 50), but it comes in after tick 5 ran.
    // It goes to the next tick, and nothing is lost or counted twice.
    REQUIRE(Rig.Frame(44 * Ms) == 0);
    const std::vector<ScriptEvent> Held{Rig.Script[10]};
    Rig.Script.resize(10); // hold the 45 ms message back
    REQUIRE(Rig.Frame(50 * Ms) == 1);
    Rig.Script.push_back(Held[0]);
    REQUIRE(Rig.Frame(52 * Ms) == 0);
    REQUIRE(Rig.Pending().MouseSamples.size() == 1);
    REQUIRE(Rig.Frame(60 * Ms) == 1);
    Rows = Rig.Rows();
    REQUIRE(Rows[4].MouseSamples == 1); // 40 ms
    REQUIRE(Rows[5].MouseSamples == 1); // 45 ms, late, in tick 6
    uint32 Total = 0;
    for (const Row& Each : Rows)
    {
        Total += Each.MouseSamples;
    }
    REQUIRE(Total == 9);
    REQUIRE(Rig.Pending().MouseSamples.empty());
}

TEST_CASE("Tick time: the ledger does not depend on the frame rate", "[time]")
{
    // The rule does not depend on the simulation rate: the default and 60 Hz
    for (const uint32 Hz : {30u, 60u})
    {
        CAPTURE(Hz);
        const SystemRunnerConfig Config{.SimHz = Hz};
        const std::vector<ScriptEvent> Script = MakeScript(7, 140);
        constexpr uint64 Ticks = 60;
        const int64 Step = Config.FixedStep().count();

        // The reference: one frame at the end of each tick's window
        TimeRig Reference(Config, Script);
        const std::vector<Row> Expected =
            RunUntil(Reference, Ticks, [Step](const uint64 f) { return static_cast<int64>(f) * Step; });

        // Every tick took exactly the script events of its window, so the rows are a function of the input only
        for (const Row& Each : Expected)
        {
            uint32 Mouse = 0;
            uint32 Key = 0;
            for (const ScriptEvent& Event : Script)
            {
                if (Event.TimeNs >= Each.Stamp.Window.StartNs && Event.TimeNs < Each.Stamp.Window.EndNs)
                {
                    (Event.Mouse ? Mouse : Key) += 1;
                }
            }
            REQUIRE(Each.MouseSamples == Mouse);
            REQUIRE(Each.KeyEvents == Key);
        }
        uint64 Changes = 0;
        for (size_t i = 1; i < Expected.size(); ++i)
        {
            Changes += Expected[i].Stamp.StateHash != Expected[i - 1].Stamp.StateHash ? 1 : 0;
        }
        REQUIRE(Changes > 10); // the hash follows the look camera

        const std::vector<std::pair<const char*, std::function<int64(uint64)>>> Patterns{
            {"3 frames per tick",
             [Step](const uint64 f)
             {
                 return static_cast<int64>(f * Step / 3);
             }},
            {"144 Hz",
             [](const uint64 f)
             {
                 return static_cast<int64>(f * 1'000'000'000 / 144);
             }},
            {"2 ticks per frame",
             [Step](const uint64 f)
             {
                 return static_cast<int64>(f) * 2 * Step;
             }},
            {"hitches",
             [](const uint64 f)
             {
                 // Uneven frames from 1 to 40 ms, and every 9th frame a hitch of 55 or 120 ms (3 or 7 ticks)
                 int64 Time = 0;
                 Lcg Random{99};
                 for (uint64 i = 0; i < f; ++i)
                 {
                     Time += (i % 9 == 8) ? (i % 2 == 0 ? 55 : 120) * Ms : Random.Range(1, 40) * Ms;
                 }
                 return Time;
             }},
        };
        for (const auto& [Name, FrameTime] : Patterns)
        {
            INFO(Name);
            TimeRig Rig(Config, Script);
            const std::vector<Row> Rows = RunUntil(Rig, Ticks, FrameTime);
            REQUIRE(Rows == Expected);
            // The camera itself, to the bit, after the same tick (the last frame can run past Ticks)
            TimeRig Same(Config, Script);
            RunUntil(Same, Rig.Runner->TickCount(), [Step](const uint64 f) { return static_cast<int64>(f) * Step; });
            REQUIRE(Same.Runner->TickCount() == Rig.Runner->TickCount());
            REQUIRE(SameBits(Same.Sim(), Rig.Sim()));
        }
    }
}

TEST_CASE("Tick time: a replay by recorded cuts gives the same ticks at another frame rate and clock speed", "[time]")
{
    const std::vector<ScriptEvent> Script = MakeScript(21, 120);
    constexpr uint64 Ticks = 60;
    const int64 Step = SystemRunnerConfig{}.FixedStep().count();
    const String Path = (std::filesystem::temp_directory_path() / "fling_tick_cuts_test.input.jsonl").string();

    // Record a live run with uneven frames
    std::vector<Row> Live;
    TestCamera LiveCamera;
    {
        TimeRig Rig(SystemRunnerConfig{}, Script);
        Rig.Recorder->Start(Path, 0.0f, 0.0f);
        Lcg Random{5};
        int64 Time = 0;
        while (Rig.Runner->TickCount() < Ticks)
        {
            Time += Random.Range(2, 30) * Ms;
            Rig.Frame(Time);
        }
        Live = Rig.Rows();
        LiveCamera = Rig.Sim();
        REQUIRE(Rig.Recorder->Stop());
    }
    InputRecording Recording;
    String Error;
    REQUIRE(LoadInputRecording(Path, Recording, Error));
    REQUIRE(Recording.TickCuts.size() == Live.size());
    // The start capture, then the script
    REQUIRE(Recording.Messages.size() == Script.size() + 1);

    // Replays: 3 frames per tick, with the playback clock far behind (the ticks must
    // pull the messages they need) and far ahead (the frame side sees them early)
    std::vector<std::vector<Row>> Replays;
    for (const double PlaybackSpeed : {0.37, 3.0})
    {
        INFO("playback speed " << PlaybackSpeed);
        TimeRig Rig;
        String StartError;
        REQUIRE(Rig.Playback->Start(Recording, 0.0f, StartError));
        REQUIRE(Rig.Playback->HasTickCuts());
        for (uint64 f = 1; Rig.Runner->TickCount() < Live.size(); ++f)
        {
            Rig.Frame(static_cast<int64>(f * Step / 3), PlaybackSpeed * static_cast<double>(Step) / 3e9);
            REQUIRE(Rig.Pending().Cut.AfterSequence == Rig.Cut());
        }
        std::vector<Row> Rows = Rig.Rows();
        Rows.resize(Live.size());
        REQUIRE(InputRows(Rows) == InputRows(Live));
        REQUIRE(SameBits(Rig.Sim(), LiveCamera));
        Replays.push_back(Rows);
    }
    // Two replays of one file also agree on the sequences (the same devices, the same order)
    for (Row& Each : Replays[0])
    {
        Each.Stamp.Window = TickWindow{};
    }
    for (Row& Each : Replays[1])
    {
        Each.Stamp.Window = TickWindow{};
    }
    REQUIRE(Replays[0] == Replays[1]);

    // An old file with no cuts still loads and plays, by time: nothing is lost
    {
        InputRecording Old = Recording;
        Old.TickCuts.clear();
        TimeRig Rig;
        String StartError;
        REQUIRE(Rig.Playback->Start(Old, 0.0f, StartError));
        REQUIRE_FALSE(Rig.Playback->HasTickCuts());
        for (uint64 f = 1; f <= 200; ++f)
        {
            Rig.Frame(static_cast<int64>(f) * Step);
        }
        uint64 Taken = 0;
        for (const Row& Each : Rig.Rows())
        {
            Taken += Each.MouseSamples + Each.KeyEvents;
        }
        REQUIRE(Taken == Script.size());
    }
    std::filesystem::remove(std::filesystem::path(Path));
}

TEST_CASE("Tick time: a pause moves the windows, and tick numbers go on", "[time]")
{
    TimeRig Rig(SystemRunnerConfig{.SimHz = 100}, {MoveAt(5 * Ms, 1), MoveAt(50 * Ms, 2), MoveAt(103 * Ms, 3)});
    REQUIRE(Rig.Frame(10 * Ms) == 1);
    Rig.Runner->SetPaused(true, 15 * Ms);
    for (int64 t = 20; t <= 100; t += 10)
    {
        REQUIRE(Rig.Frame(t * Ms) == 0);
    }
    // The move during the pause is pending, not lost
    REQUIRE(Rig.Pending().MouseSamples.size() == 1);
    Rig.Runner->SetPaused(false, 105 * Ms);
    REQUIRE(Rig.Frame(109 * Ms) == 0);
    REQUIRE(Rig.Frame(110 * Ms) == 1);
    const std::vector<Row> Rows = Rig.Rows();
    REQUIRE(Rows.size() == 2);
    REQUIRE(Rows[1].Stamp.Tick == 2);
    REQUIRE(Rows[1].Stamp.Window.StartNs == 100 * Ms);              // [10, 20) moved by the 90 ms pause
    REQUIRE(Rig.Storage.Singleton<SimTime>().ElapsedNs == 20 * Ms); // sim time does not see the pause
    REQUIRE(Rows[1].MouseSamples == 2);                             // the move during the pause, and 103 ms
}

TEST_CASE("Tick time: a long stall runs the most ticks a wake may, then drops time", "[time]")
{
    std::vector<ScriptEvent> Script;
    for (int64 t = 5; t < 1000; t += 10)
    {
        Script.push_back(MoveAt(t * Ms, 1));
    }
    const SystemRunnerConfig Config{.SimHz = 100};
    const uint64 Limit = Config.MaxTicksPerWake();
    REQUIRE(Limit == 25); // 250 ms at 10 ms a tick
    TimeRig Rig(Config, Script);
    REQUIRE(Rig.Frame(10 * Ms) == 1);
    REQUIRE(Rig.Frame(1000 * Ms) == Limit);
    REQUIRE(Rig.Frame(1005 * Ms) == 0);
    REQUIRE(Rig.Frame(1010 * Ms) == 1);

    const std::vector<Row> Rows = Rig.Rows();
    REQUIRE(Rows.size() == Limit + 2);
    for (size_t i = 0; i < Rows.size(); ++i)
    {
        REQUIRE(Rows[i].Stamp.Tick == i + 1); // no tick number is skipped
    }
    // Ticks 2 to 26 took their own windows; tick 27 runs in [1000, 1010) and takes the
    // input of the dropped time too: nothing is lost
    const size_t LastCaughtUp = Limit; // tick 26
    REQUIRE(Rows[LastCaughtUp].Stamp.Window.EndNs == 260 * Ms);
    REQUIRE(Rows[LastCaughtUp + 1].Stamp.Window.StartNs == 1000 * Ms);
    REQUIRE(Rig.Runner->GetLedger().Find(Limit + 2)->DroppedSteps == 74);
    REQUIRE(Rows[LastCaughtUp + 1].MouseSamples == 100 - 26);
    REQUIRE(Rig.Pending().MouseSamples.empty());
}

TEST_CASE("Tick time: RealFrameTime has the frame time, the ticks held and the blend factor", "[time]")
{
    TimeRig Rig(SystemRunnerConfig{.SimHz = 100});
    Rig.Frame(5 * Ms);
    REQUIRE(Rig.Shown().Tick == 0);
    Rig.Frame(25 * Ms);
    const RealFrameTime& Frame = Rig.Storage.Singleton<RealFrameTime>();
    REQUIRE(Frame.FrameNs == 25 * Ms);
    REQUIRE(Frame.Latest == Rig.Runner->GetLedger().Find(2)->Stamp);
    REQUIRE(Frame.Latest.Window == TickWindow{10 * Ms, 20 * Ms, TickTrigger::Clock});
    // A runner with no presentation link holds one tick: nothing to blend from
    REQUIRE(Frame.BlendFrom == Frame.Latest);
    REQUIRE(Frame.BlendAlpha == 1.0f);
    // The frame side's blend of ticks 1 and 2 (FrameBlend): a frame at tick 1, then tick 2 arrives
    const TickStamp& Tick1 = Rig.Runner->GetLedger().Find(1)->Stamp;
    const TickStamp& Tick2 = Rig.Runner->GetLedger().Find(2)->Stamp;
    const std::vector<PacketTable> NoTables;
    FrameBlend Blend;
    Blend.SetTypes({});
    Blend.Show(ShownTicks{Tick1, &NoTables});
    Rig.Runner->RunFrameAt(20 * Ms, 1ms, WorldRole::Play, Tick1, Blend.Advance(20 * Ms));
    REQUIRE(Frame.BlendFrom == Tick1);
    REQUIRE(Frame.BlendAlpha == 1.0f);
    Blend.Show(ShownTicks{Tick2, &NoTables, Tick1, &NoTables});
    Rig.Runner->RunFrameAt(25 * Ms, 1ms, WorldRole::Play, Tick2, Blend.Advance(25 * Ms));
    REQUIRE(Frame.BlendFrom == Tick1);
    REQUIRE(Frame.BlendTo == Tick2);
    REQUIRE(Frame.BlendAlpha == 0.5f);
    Rig.Runner->RunFrameAt(30 * Ms, 1ms, WorldRole::Play, Tick2, Blend.Advance(30 * Ms));
    REQUIRE(Frame.BlendAlpha == 1.0f);
    // Edit shows the latest state
    Rig.Runner->RunFrameAt(30 * Ms, 1ms, WorldRole::Edit, Tick2);
    REQUIRE(Frame.BlendTo == Tick2);
    REQUIRE(Frame.BlendAlpha == 1.0f);
    REQUIRE(Rig.Storage.Singleton<SimTime>().Tick == 2);
    REQUIRE(Rig.Storage.Singleton<SimTime>().ElapsedNs == 20 * Ms);
    REQUIRE(Rig.Storage.Singleton<SimTime>().DeltaSeconds() == 0.01f);
}

TEST_CASE("Tick time: the ledger keeps the last 256 ticks", "[time]")
{
    TimeRig Rig(SystemRunnerConfig{.SimHz = 100});
    Rig.Frame(10 * Ms);
    const TickLedger& Ledger = Rig.Runner->GetLedger();
    for (int i = 0; i < 299; ++i)
    {
        Rig.Runner->RunTick(WorldRole::Play);
        // Tick 0 means no tick: never found, also while its slot (0) holds no tick yet
        if (i == 100)
        {
            REQUIRE(Ledger.Find(0) == nullptr);
        }
    }
    REQUIRE(Ledger.Size() == TickLedger::Capacity);
    REQUIRE(Ledger.TotalWritten() == 300);
    REQUIRE(Ledger.At(0).Stamp.Tick == 300 - TickLedger::Capacity + 1);
    REQUIRE(Ledger.Latest()->Stamp.Tick == 300);
    REQUIRE(Ledger.Find(300 - TickLedger::Capacity) == nullptr);
    REQUIRE(Ledger.Find(100)->Stamp.Tick == 100);
    REQUIRE(Ledger.Find(100)->Stamp.Window.Trigger == TickTrigger::Request); // RunTick runs on request
    REQUIRE(Ledger.Find(301) == nullptr);
}

namespace
{
    /** Game state that is not saved to a level: a rollback restores it, so the hash covers it. */
    struct HashedScore
    {
        static constexpr ComponentInfo Component{.Name = "TickTimeTest.HashedScore",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int32 Points = 0;
        /** A Transient field: a level does not save it, but a restore brings it back. */
        int32 Streak = 0;
    };

    /** Restored, with a member that no field reflects: the hash would not see it. */
    struct HalfReflected
    {
        static constexpr ComponentInfo Component{.Name = "TickTimeTest.HalfReflected",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int32 Seen = 0;
        int32 Unseen = 0;
    };

    /** The same members, but a rollback does not restore it: not in the restore set, no error. */
    struct HalfReflectedScratch
    {
        static constexpr ComponentInfo Component{.Name = "TickTimeTest.HalfReflectedScratch",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Keep};
        int32 Seen = 0;
        int32 Unseen = 0;
    };
} // namespace

TEST_CASE("Tick time: the state hash covers exactly what a rollback restores", "[time]")
{
    SystemRegistry Systems;
    ModuleRegistration Modules(Systems);
    TestLook::RegisterCameraType(Modules);
    Modules.Component<CameraView>();
    Modules.Component<InputState>();
    Modules.Component<SimTime>();
    Modules.Component<EntityGuid>([](TypeBuilder<EntityGuid>& t_Type) { t_Type.Field("value", &EntityGuid::Value); });
    Modules.Component<HashedScore>(
        [](TypeBuilder<HashedScore>& t_Type)
        {
            t_Type.Field("points", &HashedScore::Points);
            t_Type.Field("streak", &HashedScore::Streak).Transient();
        });
    const TypeRegistry Types = TypeRegistry::Build(Modules);
    REQUIRE(Types.Ok());

    TestCamera A;
    A.Yaw = 0.5f;
    TestCamera B;
    B.Pitch = -0.25f;

    // The same entities and values, made in two ways (one archetype move or none)
    EntityStorage First;
    First.Create(A);
    const Entity Second = First.Create();
    First.Add<TestCamera>(Second, B);
    First.SetSingleton<HashedScore>();
    First.SetSingleton<CameraView>();
    First.SetSingleton<InputState>();
    First.SetSingleton<SimTime>();
    EntityStorage Other;
    Other.Create(A);
    Other.Create(B);
    Other.SetSingleton<HashedScore>();
    Other.SetSingleton<CameraView>();
    Other.SetSingleton<InputState>();
    Other.SetSingleton<SimTime>();
    REQUIRE(HashSimulationState(First, Types) == HashSimulationState(Other, Types));

    // Not rollback state: a Presentation component, the Rebuild singletons that the runner
    // writes for each tick (a resimulation writes them again), and the file id of an entity
    First.Singleton<CameraView>().Yaw = 3.0f;
    First.Add<EntityGuid>(Second, EntityGuid{0x5EED});
    First.Singleton<SimTime>().Tick = 9;
    First.Singleton<InputState>().Tick.Slice.Cut = InputCut{1, 2};
    REQUIRE(HashSimulationState(First, Types) == HashSimulationState(Other, Types));

    // Rollback state: a saved field, and a Transient singleton of class Restore
    First.Get<TestCamera>(Second).Frozen = true;
    const uint64 Frozen = HashSimulationState(First, Types);
    REQUIRE(Frozen != HashSimulationState(Other, Types));
    First.Singleton<HashedScore>().Points = 3;
    const uint64 Scored = HashSimulationState(First, Types);
    REQUIRE(Scored != Frozen);

    // A Transient field of a restored type: not saved to a level, but restored, so hashed
    First.Singleton<HashedScore>().Streak = 2;
    REQUIRE(HashSimulationState(First, Types) != Scored);
}

namespace
{
    /** A restored type with padding on every platform: 7 bytes after On, 3 after Other. */
    struct PaddedState
    {
        static constexpr ComponentInfo Component{
            .Name = "TickTimeTest.PaddedState", .Transient = true, .Rollback = RollbackClass::Restore};
        bool On = false;
        double Amount = 0.0;
        bool Other = false;
        float Speed = 0.0f;
        glm::vec3 Where{0.0f};
    };
    static_assert(offsetof(PaddedState, Amount) > sizeof(bool) &&
                  offsetof(PaddedState, Speed) > offsetof(PaddedState, Other) + 1);
} // namespace

TEST_CASE("Tick time: the state hash reads values and never padding on every platform", "[time]")
{
    // The hash reads each registered field by value (a vec3 as x, y, z), never the bytes of an object. So padding
    // never reaches it, and the hash does not depend on layout: GLM types are packed on every platform
    // (FlingMath.h), but a layout that drifted would still hash the same values the same way.
    SystemRegistry Systems;
    ModuleRegistration Modules(Systems);
    Modules.Component<Transform>(
        [](TypeBuilder<Transform>& t_Type)
        {
            t_Type.Field("position", &Transform::Position);
            t_Type.Field("rotation", &Transform::Rotation);
            t_Type.Field("scale", &Transform::Scale);
        });
    Modules.Component<PaddedState>(
        [](TypeBuilder<PaddedState>& t_Type)
        {
            t_Type.Field("on", &PaddedState::On);
            t_Type.Field("amount", &PaddedState::Amount);
            t_Type.Field("other", &PaddedState::Other);
            t_Type.Field("speed", &PaddedState::Speed);
            t_Type.Field("where", &PaddedState::Where);
        });
    const TypeRegistry Types = TypeRegistry::Build(Modules);
    REQUIRE(Types.Ok());

    // Every byte of each object, padding included, filled with t_Byte before the values are set
    const auto MakeTransform = [](const unsigned char t_Byte)
    {
        alignas(Transform) unsigned char Bytes[sizeof(Transform)];
        std::memset(Bytes, t_Byte, sizeof(Bytes));
        Transform Value;
        std::memcpy(&Value, Bytes, sizeof(Value));
        Value.Position = {1.0f, -2.0f, 3.5f};
        Value.Rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        Value.Scale = {2.0f, 2.0f, 2.0f};
        return Value;
    };
    const auto MakePadded = [](const unsigned char t_Byte)
    {
        alignas(PaddedState) unsigned char Bytes[sizeof(PaddedState)];
        std::memset(Bytes, t_Byte, sizeof(Bytes));
        PaddedState Value;
        std::memcpy(&Value, Bytes, sizeof(Value));
        Value.On = true;
        Value.Amount = 0.25;
        Value.Other = false;
        Value.Speed = -4.5f;
        Value.Where = {7.0f, 8.0f, 9.0f};
        return Value;
    };
    const auto Fill = [&](EntityStorage& t_Storage, const unsigned char t_Byte)
    {
        const Entity Made = t_Storage.Create(MakeTransform(t_Byte));
        t_Storage.Add<PaddedState>(Made, MakePadded(t_Byte));
        return Made;
    };
    EntityStorage First;
    Fill(First, 0x00);
    EntityStorage Other;
    const Entity Moved = Fill(Other, 0xCD);
    const uint64 Hash = HashSimulationState(First, Types);
    REQUIRE(Hash == HashSimulationState(Other, Types));

    // Fixed value: the same on Windows and Linux, whatever the layout. A server and a client on two platforms
    // compare these hashes, so a failure here on one platform means that platform hashes differently.
    CHECK(Hash == 0x94B558F163C0C2BDull);

    // Each field is still hashed
    Other.Get<Transform>(Moved).Position.z = 3.25f;
    REQUIRE(HashSimulationState(First, Types) != HashSimulationState(Other, Types));
    Other.Get<Transform>(Moved).Position.z = 3.5f;
    REQUIRE(HashSimulationState(Other, Types) == Hash);
    Other.Get<PaddedState>(Moved).Other = true;
    REQUIRE(HashSimulationState(First, Types) != HashSimulationState(Other, Types));
}

TEST_CASE("Tick time: a restored type with members the hash cannot see is an error", "[time]")
{
    SystemRegistry Systems;
    ModuleRegistration Modules(Systems);
    Modules.Component<HalfReflected>([](TypeBuilder<HalfReflected>& t_Type)
                                     { t_Type.Field("seen", &HalfReflected::Seen); });
    // Partial() says the other members are left out on purpose: for a level, not for a rollback
    Modules.Component<HalfReflectedScratch>([](TypeBuilder<HalfReflectedScratch>& t_Type)
                                            { t_Type.Partial().Field("seen", &HalfReflectedScratch::Seen); });
    const TypeRegistry Types = TypeRegistry::Build(Modules);
    REQUIRE_FALSE(Types.Ok());
    REQUIRE(Types.Errors().size() == 1);
    REQUIRE(Types.Errors().front().find("'TickTimeTest.HalfReflected' is restored by a rollback") != String::npos);
}
