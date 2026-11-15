// Tests of the exec modes (Docs/threading-model.md section 9). In every test this thread is the
// input writer and Lane::Frame, as in the engine; in Threaded mode the game thread cuts
// the input by the timeline. The tick ledger is the same in Direct, Inline and Threaded play
// at any frame pattern, ticks per wake and hitch; a free game thread gives the same ledger
// (the watermark rule); live play with no wait replays exactly from its recorded cuts; the
// frame side never sees a cut that does not match its data; clock commands, a Quit and a
// shutdown during play behave the same in every mode; a simulation rate change applies at
// one tick boundary in every mode and is refused during a replay or a recording; and the
// lane checks.

#include "../Common/TestScopes.h"
#include "MathUtils.h"
#include "RunControls.h"
#include "ThreadedWorldHarness.h"
#include "Threading/Lane.h"

#include <algorithm>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <filesystem>
#include <functional>
#include <utility>
#include <vector>

using namespace ThreadedTest;
using namespace std::chrono_literals;

namespace
{
    /** How a run spreads its ticks over wakes and frames. */
    enum class Pattern : uint8
    {
        /** One tick per wake, one frame after each. */
        OneFramePerTick,
        /** One tick per wake, three frames after each. */
        ThreeFramesPerTick,
        /** Bursts of 1 to 7 ticks in one wake, one frame after each. */
        Bursts,
        /** One tick per wake, no frames at all. */
        NoFrames,
        /** One tick per wake, and every tenth wake a hitch: 12 ticks of time, of which 8 run and 4 are dropped. */
        Hitches,
    };

    /** Run t_Ticks ticks in a pattern. Every frame checks the shown state. Returns the ledger rows. */
    std::vector<Row> RunPattern(HarnessOptions t_Options, const Pattern t_Pattern, const uint64 t_Ticks)
    {
        // The main thread is Lane::Frame, as in the engine
        const LaneCheck::Scope MainLane(Lane::Frame);
        ThreadedWorldHarness Harness(t_Options);
        World& TheWorld = Harness.GetWorld();
        Harness.StartPlay();
        uint32 Wake = 0;
        while (TheWorld.GetTickCount() < t_Ticks)
        {
            ++Wake;
            const uint64 Left = t_Ticks - TheWorld.GetTickCount();
            uint64 Ticks = 1;
            if (t_Pattern == Pattern::Bursts)
            {
                Ticks = Wake % 7 + 1;
            }
            else if (t_Pattern == Pattern::Hitches && Wake % 10 == 0)
            {
                // More than one wake may catch up: the rest of the time is dropped
                Ticks = SystemRunnerConfig{.SimHz = t_Options.SimHz}.MaxTicksPerWake() + 4;
            }
            Harness.StepTicks(static_cast<uint32>(Math::Min(Ticks, Left)));
            const int Frames = t_Pattern == Pattern::ThreeFramesPerTick ? 3 : (t_Pattern == Pattern::NoFrames ? 0 : 1);
            for (int f = 0; f < Frames; ++f)
            {
                Harness.StepFrame();
                Harness.CheckShownState();
            }
        }
        Harness.StepFrame();
        Harness.CheckShownState();
        REQUIRE(TheWorld.GetPresentedStorage().Singleton<RealFrameTime>().Latest.Tick == t_Ticks);
        Harness.StopPlay();
        return Harness.Rows();
    }

    HarnessOptions InMode(const ExecMode t_Mode, HarnessOptions t_Options = {})
    {
        t_Options.Mode = t_Mode;
        return t_Options;
    }

    /**
     * Threaded play with a free game thread: this thread moves the clock and polls (at most a
     * few ticks ahead, as a headless frame does), and runs a frame after each step, until the
     * frame side shows the last tick. Returns the ledger rows; with RecordTo, t_OutRecording
     * gets the recording of the run.
     */
    std::vector<Row>
    RunFreeGameThread(HarnessOptions t_Options, const uint64 t_Ticks, InputRecording* t_OutRecording = nullptr)
    {
        const LaneCheck::Scope MainLane(Lane::Frame);
        t_Options.Mode = ExecMode::Threaded;
        t_Options.Pacing = GameLanePacing::AsFastAsPossible;
        t_Options.LastTick = t_Ticks;
        ThreadedWorldHarness Harness(t_Options);
        Harness.StartPlay();

        const auto Start = std::chrono::steady_clock::now();
        uint64 Frames = 0;
        uint64 LastShown = 0;
        while (Harness.GetWorld().GetShownTick() < t_Ticks)
        {
            Harness.AdvanceClockAhead(4);
            Harness.StepFrame();
            Harness.CheckShownState();
            const uint64 Shown = Harness.GetWorld().GetShownTick();
            REQUIRE(Shown >= LastShown);
            LastShown = Shown;
            ++Frames;
            REQUIRE(std::chrono::steady_clock::now() - Start < 60s);
        }
        const PublishChannel& Channel = *Harness.GetWorld().GetPublishChannel();
        UNSCOPED_INFO("frames " << Frames << ", packets published " << Channel.PublishCount() << ", applied "
                                << Channel.ApplyCount());
        // Both threads did work
        REQUIRE(Frames > 10);
        REQUIRE(Channel.ApplyCount() > 1);
        Harness.StopPlay();
        const std::vector<Row> Rows = Harness.Rows();
        REQUIRE(Rows.size() == t_Ticks);
        if (t_OutRecording != nullptr)
        {
            *t_OutRecording = Harness.LoadRecording();
        }
        return Rows;
    }
} // namespace

TEST_CASE("Exec modes: the tick ledger is the same in Direct, Inline and Threaded", "[threads]")
{
    // Mutation note: catches a tick that takes input by anything but its window (the time
    // rule in TickInputProvider::Build: a tick that takes every queued message differs in
    // Bursts and Threaded), and a simulation that reads the frame side or the thread. Checked
    // by breaking the time rule (Limit.UntilNs not set): this case fails.
    const TestScopes::CountLaneViolations Violations;
    constexpr uint64 Ticks = 240;
    // The rule does not depend on the simulation rate: the default and 60 Hz
    for (const uint32 Hz : {30u, 60u})
    {
        CAPTURE(Hz);
        HarnessOptions AtRate;
        AtRate.SimHz = Hz;
        const std::vector<Row> Reference =
            RunPattern(InMode(ExecMode::Direct, AtRate), Pattern::OneFramePerTick, Ticks);
        REQUIRE(Reference.size() == Ticks);

        // The script moves the camera: the hash changes, and the ticks take input
        uint32 HashChanges = 0;
        uint32 Samples = 0;
        for (size_t i = 1; i < Reference.size(); ++i)
        {
            HashChanges += Reference[i].StateHash != Reference[i - 1].StateHash ? 1 : 0;
            Samples += Reference[i].MouseSamples;
        }
        REQUIRE(HashChanges > 50);
        REQUIRE(Samples > 100);

        // The frames and the ticks per wake do not change the ledger
        for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
        {
            for (const Pattern Each :
                 {Pattern::OneFramePerTick, Pattern::ThreeFramesPerTick, Pattern::Bursts, Pattern::NoFrames})
            {
                CAPTURE(ExecModeName(Mode), static_cast<int>(Each));
                REQUIRE(RunPattern(InMode(Mode, AtRate), Each, Ticks) == Reference);
            }
        }

        // A hitch drops time, so the ledger differs from the reference, but it is the same in every mode
        const std::vector<Row> HitchReference = RunPattern(InMode(ExecMode::Direct, AtRate), Pattern::Hitches, Ticks);
        REQUIRE(HitchReference != Reference);
        for (const ExecMode Mode : {ExecMode::Inline, ExecMode::Threaded})
        {
            CAPTURE(ExecModeName(Mode));
            REQUIRE(RunPattern(InMode(Mode, AtRate), Pattern::Hitches, Ticks) == HitchReference);
        }
    }
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: at 1000 Hz, 60 fps frames run every tick of the clock time", "[threads]")
{
    // A wake that spans more ticks than a fixed count must not drop time: one second of
    // 60 fps frames is 1000 ticks in every mode
    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        CAPTURE(ExecModeName(Mode));
        const LaneCheck::Scope MainLane(Lane::Frame);
        HarnessOptions Options = InMode(Mode);
        Options.SimHz = 1000;
        ThreadedWorldHarness Harness(Options);
        Harness.StartPlay();
        constexpr int64 FrameNs = 16'666'667;
        for (int Frame = 0; Frame < 60; ++Frame)
        {
            Harness.StepTime(FrameNs);
            Harness.StepFrame();
        }
        REQUIRE(Harness.GetWorld().GetTickCount() == 1000);
        Harness.StopPlay();
        const std::vector<Row> Rows = Harness.Rows();
        REQUIRE(Rows.size() == 1000);
        REQUIRE(Rows.back().Tick == 1000);
    }
}

TEST_CASE("Exec modes: the main thread writes the input and a free game thread cuts it by the timeline",
          "[threads][stress]")
{
    // The watermark rule: a timed tick on the game thread waits until the main thread has
    // polled up to the end of its window, so its cut is the one Direct gives.
    // Mutation note: catches a game thread that does not wait for the writer
    // (TickInputProvider::WaitForWatermark, or World::MakeTickInputConfig without the
    // Writer sender or the wait). Checked by skipping the wait: this case fails.
    const TestScopes::CountLaneViolations Violations;
    constexpr uint64 Ticks = 30000;
    HarnessOptions Options;
    Options.ScriptEvents = 30000;
    const std::vector<Row> Reference = RunPattern(InMode(ExecMode::Direct, Options), Pattern::NoFrames, Ticks);

    for (const uint32 Jitter : {0u, 300u})
    {
        CAPTURE(Jitter);
        // The jitter waits on both threads: fewer ticks keep the run to a few seconds
        const uint64 RunTicks = Jitter > 0 ? 5000 : Ticks;
        HarnessOptions Free = Options;
        Free.JitterMaxMicroseconds = Jitter;
        const std::vector<Row> Rows = RunFreeGameThread(Free, RunTicks);
        REQUIRE(std::equal(Rows.begin(), Rows.end(), Reference.begin()));
    }
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: a main thread that runs ahead of the game thread past the input queue marks the tick",
          "[threads]")
{
    // The main thread never waits for the game thread, so in headless threaded play it can
    // queue more input than the ring holds before a tick takes it. The tick then loses input,
    // and its ledger record says so. Mutation note: fails if SystemRunner::RunOneTick does
    // not copy InputSlice::Overran into TickRecord::InputOverran.
    const TestScopes::CountLaneViolations Violations;
    const LaneCheck::Scope MainLane(Lane::Frame);
    HarnessOptions Options = InMode(ExecMode::Threaded);
    Options.ScriptEvents = static_cast<int>(InputMessageQueue::DefaultCapacity) + 500;
    ThreadedWorldHarness Harness(Options);
    Harness.StartPlay();

    // One frame queues the whole script (about 25 s of input); the game thread has not run
    Harness.StepTime(60'000'000'000);
    Harness.StepTicks(1);
    const std::vector<TickRecord>& Records = Harness.Records();
    REQUIRE(Records.size() >= 2);
    REQUIRE(Records.front().InputOverran);
    REQUIRE(std::none_of(Records.begin() + 1, Records.end(), [](const TickRecord& t_R) { return t_R.InputOverran; }));
    Harness.StopPlay();
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: live play that does not wait for input replays exactly from its recorded cuts", "[threads]")
{
    // The live rule: with no watermark wait, a tick takes what the main thread has polled when
    // it runs, so a cut can be short of the time rule's (the rest goes to the next tick). The
    // recorder notes each cut (InputRecorder::NoteTickCut); a replay of the recording
    // (InputPlayback, the recorded cut per tick) gives every tick the same input and state,
    // in every mode, through the engine's input path.
    // Mutation note: catches a replay that does not take the recorded cut (the replay rule in
    // TickInputProvider::TakeRecordedCut), a recorder that drops or moves a cut, and a
    // playback that sends a cut's messages late. Checked by making TakeRecordedCut return no
    // cut: this case fails.
    const TestScopes::CountLaneViolations Violations;
    constexpr uint64 Ticks = 3000;
    HarnessOptions Options;
    Options.ScriptEvents = 3000;
    const std::vector<Row> TimeRule = RunPattern(InMode(ExecMode::Direct, Options), Pattern::NoFrames, Ticks);

    HarnessOptions Live = Options;
    Live.WatermarkWaitNs = 0;
    Live.JitterMaxMicroseconds = 200;
    Live.RecordTo = (std::filesystem::temp_directory_path() / "fling-threaded-live-replay.input.jsonl").string();
    InputRecording Recording;
    const uint64 LocksBeforeLive = InputPlayback::GetCutTableLockCount();
    const std::vector<Row> Recorded = RunFreeGameThread(Live, Ticks, &Recording);
    // Live play shares no mutex between the threads: neither takes the playback's cut lock
    REQUIRE(InputPlayback::GetCutTableLockCount() == LocksBeforeLive);
    REQUIRE(Recording.TickCuts.size() == Ticks);
    uint32 ShortCuts = 0;
    for (size_t i = 0; i < Ticks; ++i)
    {
        // A live tick never takes a message of a later window
        REQUIRE(Recorded[i].LastSequence <= TimeRule[i].LastSequence);
        ShortCuts += Recorded[i].LastSequence < TimeRule[i].LastSequence ? 1 : 0;
    }
    UNSCOPED_INFO("live ticks with a short cut: " << ShortCuts << " of " << Ticks);

    HarnessOptions Replay = Options;
    Replay.Replay = Recording;
    std::vector<Row> FirstReplay;
    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        for (const Pattern Each : {Pattern::OneFramePerTick, Pattern::Bursts})
        {
            CAPTURE(ExecModeName(Mode), static_cast<int>(Each));
            const std::vector<Row> Rows = RunPattern(InMode(Mode, Replay), Each, Ticks);
            REQUIRE(SameInputAndState(Rows, Recorded));
            // Every replay is the same to the sequence, in every mode and frame pattern
            if (FirstReplay.empty())
            {
                FirstReplay = Rows;
            }
            REQUIRE(Rows == FirstReplay);
        }
    }
    // A replay hands its cuts to the tick lane under that lock
    REQUIRE(InputPlayback::GetCutTableLockCount() > LocksBeforeLive);
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: threaded play stops and starts again, and Edit runs on this thread", "[threads]")
{
    const TestScopes::CountLaneViolations Violations;
    const LaneCheck::Scope MainLane(Lane::Frame);
    ThreadedWorldHarness Harness(InMode(ExecMode::Threaded));
    World& TheWorld = Harness.GetWorld();

    // Edit: Direct, on this thread
    Harness.StepTicks(3);
    REQUIRE(TheWorld.GetGameLane() == nullptr);
    REQUIRE_FALSE(TheWorld.IsPresentingCopy());
    REQUIRE(TheWorld.GetTickCount() == 3);

    for (int Session = 1; Session <= 2; ++Session)
    {
        Harness.StartPlay();
        REQUIRE(TheWorld.GetGameLane() != nullptr);
        REQUIRE(TheWorld.IsPresentingCopy());
        REQUIRE(&TheWorld.GetPresentedStorage() != &TheWorld.GetStorage());
        // The first packet is the state play starts with
        Harness.StepFrame();
        REQUIRE(TheWorld.GetPresentedStorage().Count<TestCamera>() == 1);

        Harness.StepTicks(5);
        Harness.StepTicks(5);
        Harness.StepFrame();
        Harness.CheckShownState();

        Harness.StopPlay();
        REQUIRE(TheWorld.GetGameLane() == nullptr);
        REQUIRE_FALSE(TheWorld.IsPresentingCopy());
        // The snapshot from before play is back: the camera that play made is gone
        REQUIRE(TheWorld.GetStorage().Count<TestCamera>() == 0);
        REQUIRE(Harness.GetGame().Starts == Session);
        REQUIRE(Harness.GetGame().Stops == Session);

        // Edit again, on this thread
        Harness.StepTicks(2);
    }
    REQUIRE(TheWorld.GetTickCount() == 3 + 2 * (10 + 2));
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: the frame systems begin play on the state that play starts with", "[threads]")
{
    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        CAPTURE(ExecModeName(Mode));
        const LaneCheck::Scope MainLane(Lane::Frame);
        ThreadedWorldHarness Harness(InMode(Mode));
        Harness.StartPlay();
        REQUIRE(GetTickLog().CamerasAtFramePlayBegin == 1);
        Harness.StopPlay();
    }
}

TEST_CASE("Exec modes: clock commands pause the ticks at the same tick in every mode", "[threads]")
{
    std::vector<Row> Reference;
    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        CAPTURE(ExecModeName(Mode));
        const TestScopes::CountLaneViolations Violations;
        const LaneCheck::Scope MainLane(Lane::Frame);
        ThreadedWorldHarness Harness(InMode(Mode));
        World& TheWorld = Harness.GetWorld();
        Harness.StartPlay();

        // A live pause: at the next wake. Wakes go on, ticks do not, and the clock moves on
        Harness.StepTicks(3);
        TheWorld.PauseTicks();
        const uint64 WakesBefore = TheWorld.GetGameLane() ? TheWorld.GetGameLane()->WakeCount() : 0;
        for (int i = 0; i < 5; ++i)
        {
            Harness.StepTicks(1);
        }
        REQUIRE(TheWorld.GetTickCount() == 3);
        if (const GameLane* Lane = TheWorld.GetGameLane())
        {
            REQUIRE(Lane->WakeCount() == WakesBefore + 5);
        }
        TheWorld.ResumeTicks();
        Harness.StepTicks(1);
        Harness.StepTicks(4);
        const uint64 AfterResume = TheWorld.GetTickCount();
        REQUIRE(AfterResume > 3);

        // A stamped pause: no tick past the one before it, then the pause at that boundary
        TheWorld.SendClockCommand(ClockCommand{ClockCommandType::Pause, AfterResume + 3});
        Harness.StepTicks(8);
        REQUIRE(TheWorld.GetTickCount() == AfterResume + 2);
        Harness.StepTicks(2);
        REQUIRE(TheWorld.GetTickCount() == AfterResume + 2);

        // A stamped command whose tick already ran is a repeat: dropped
        TheWorld.SendClockCommand(ClockCommand{ClockCommandType::Resume, 2});
        Harness.StepTicks(2);
        REQUIRE(TheWorld.GetTickCount() == AfterResume + 2);
        TheWorld.ResumeTicks();
        Harness.StepTicks(1);
        Harness.StepTicks(3);
        REQUIRE(TheWorld.GetTickCount() > AfterResume + 2);
        Harness.StopPlay();

        // The numbers go on after each pause, and the ledger is the same in every mode
        const std::vector<Row> Rows = Harness.Rows();
        for (size_t i = 0; i < Rows.size(); ++i)
        {
            REQUIRE(Rows[i].Tick == i + 1);
        }
        if (Reference.empty())
        {
            Reference = Rows;
        }
        REQUIRE(Rows == Reference);
        REQUIRE(TestScopes::CountLaneViolations::Count == 0);
    }
}

namespace
{
    /** What a run with a rate change gave each tick: its ledger record and the SimTime its systems saw. */
    struct RateRun
    {
        std::vector<Row> Rows;
        std::vector<TickRecord> Records;
        std::vector<TickSeen> Seen;
    };

    /**
     * Play at t_FromHz with a SetRate command to t_ToHz stamped before tick t_AtTick, on frames
     * of t_FrameNs (not a whole number of ticks at either rate) for t_Frames frames.
     */
    RateRun RunRateChange(const ExecMode t_Mode,
                          const uint32 t_FromHz,
                          const uint32 t_ToHz,
                          const uint64 t_AtTick,
                          const int64 t_FrameNs,
                          const int t_Frames)
    {
        const LaneCheck::Scope MainLane(Lane::Frame);
        HarnessOptions Options = InMode(t_Mode);
        Options.SimHz = t_FromHz;
        Options.MaxTicks = 2000;
        ThreadedWorldHarness Harness(Options);
        Harness.StartPlay();
        String Error;
        REQUIRE(Harness.GetWorld().RequestSimHz(t_ToHz, Error, t_AtTick));
        for (int Frame = 0; Frame < t_Frames; ++Frame)
        {
            Harness.StepTime(t_FrameNs);
            Harness.CheckShownState();
        }
        Harness.StopPlay();
        REQUIRE(Harness.GetWorld().GetSimHz() == t_ToHz);

        RateRun Run{Harness.Rows(), Harness.Records(), {}};
        for (const Row& R : Run.Rows)
        {
            Run.Seen.push_back(GetTickLog().Seen[R.Tick]);
        }
        return Run;
    }
} // namespace

TEST_CASE("Exec modes: a simulation rate change applies at one tick boundary, with the same ledger in every mode",
          "[threads]")
{
    // Mutation note: checks every window, FixedDeltaNs and ElapsedNs against the rate of its
    // tick. Checked by breaking TickTimeline::SetStep (no rebase: the windows fail) and the
    // ElapsedNs base in SystemRunner::SetSimHz (ElapsedNs fails). The ledger row alone gives
    // each tick's SimTime: checked by a stamp whose ElapsedNs leaves out the rate base.
    struct Change
    {
        uint32 From;
        uint32 To;
    };
    for (const Change Rates : {Change{30, 60}, Change{60, 20}})
    {
        constexpr uint64 AtTick = 25;
        const int64 FromNs = SimRate::StepNs(Rates.From);
        const int64 ToNs = SimRate::StepNs(Rates.To);
        std::optional<RateRun> Reference;
        for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
        {
            CAPTURE(Rates.From, Rates.To, ExecModeName(Mode));
            const TestScopes::CountLaneViolations Violations;
            const RateRun Run = RunRateChange(Mode, Rates.From, Rates.To, AtTick, 7'300'000, 400);
            REQUIRE(Run.Records.size() > AtTick + 10);

            int64 ExpectedElapsed = 0;
            for (size_t i = 0; i < Run.Records.size(); ++i)
            {
                const TickRecord& Record = Run.Records[i];
                const uint64 Tick = Record.Stamp.Tick;
                CAPTURE(Tick);
                // The numbers never skip, and each window starts where the one before ended
                REQUIRE(Tick == i + 1);
                if (i > 0)
                {
                    REQUIRE(Record.Stamp.Window.StartNs == Run.Records[i - 1].Stamp.Window.EndNs);
                }
                const int64 StepNs = Tick < AtTick ? FromNs : ToNs;
                REQUIRE(Record.Stamp.Window.EndNs - Record.Stamp.Window.StartNs == StepNs);
                REQUIRE(Run.Seen[i].Time.FixedDeltaNs == StepNs);
                ExpectedElapsed += StepNs;
                REQUIRE(Run.Seen[i].Time.ElapsedNs == ExpectedElapsed);
                // What a resimulation of this tick would write, from its ledger row only
                REQUIRE(SimTimeOf(Record.Stamp) == Run.Seen[i].Time);
            }
            REQUIRE(Run.Records[AtTick - 1].Stamp.Window.StartNs == static_cast<int64>(AtTick - 1) * FromNs);

            // The input still reaches the ticks, and every mode gives the same ledger and windows
            REQUIRE(std::ranges::any_of(Run.Rows, [](const Row& t_Row) { return t_Row.MouseSamples > 0; }));
            if (!Reference)
            {
                Reference = Run;
            }
            REQUIRE(Run.Rows == Reference->Rows);
            for (size_t i = 0; i < Run.Records.size(); ++i)
            {
                REQUIRE(Run.Records[i].Stamp.Window == Reference->Records[i].Stamp.Window);
            }
            REQUIRE(TestScopes::CountLaneViolations::Count == 0);
        }
    }
}

TEST_CASE("Exec modes: a live rate request applies at the next wake; a replay or a recording refuses it", "[threads]")
{
    const TestScopes::CountLaneViolations Violations;
    const LaneCheck::Scope MainLane(Lane::Frame);
    const String RecordPath =
        (std::filesystem::temp_directory_path() / "fling-rate-change-refused.input.jsonl").string();
    InputRecording Recording;
    String Error;

    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        CAPTURE(ExecModeName(Mode));

        // While a recording runs: refused, and the rate stays
        {
            HarnessOptions Options = InMode(Mode);
            Options.RecordTo = RecordPath;
            ThreadedWorldHarness Harness(Options);
            RunSettings Settings;
            RunControls Controls(Settings, Harness.GetWorld());
            Harness.StartPlay();
            Harness.StepTicks(5);
            REQUIRE_FALSE(Controls.RequestSimHz(60, Error));
            REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("recording is running"));
            Harness.StepTicks(5);
            Harness.StopPlay();
            REQUIRE(Harness.GetWorld().GetSimHz() == 30);
            for (const TickRecord& Record : Harness.Records())
            {
                REQUIRE(Record.Stamp.Window.EndNs - Record.Stamp.Window.StartNs == SimRate::StepNs(30));
            }
            Recording = Harness.LoadRecording();
            REQUIRE(Recording.TickCutRate == 30);
        }

        // A live request with no replay and no recording: the next wake runs at 60 Hz, and the
        // input recorder and playback get the new rate on the next frame
        {
            ThreadedWorldHarness Harness(InMode(Mode));
            RunSettings Settings;
            RunControls Controls(Settings, Harness.GetWorld());
            REQUIRE_FALSE(Controls.RequestSimHz(0, Error));
            REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("from 1 to 1000"));
            REQUIRE_FALSE(Controls.RequestSimHz(1001, Error));

            Harness.StartPlay();
            Harness.StepTicks(4);
            const uint64 Before = Harness.GetWorld().GetTickCount();
            REQUIRE(Controls.RequestSimHz(60, Error));
            REQUIRE(Error.empty());
            Harness.StepTime(SimRate::StepNs(30));
            Controls.OnFrame(PlatformTime::NowNanoseconds());
            REQUIRE(Harness.GetWorld().GetSimHz() == 60);
            REQUIRE(Controls.SimHz() == 60);
            REQUIRE(Input::Get().GetRecorder().GetTickRate() == 60);
            REQUIRE(Input::Get().GetPlayback().GetTickRate() == 60);
            Harness.StepTime(SimRate::StepNs(30));
            Harness.StopPlay();
            const std::vector<TickRecord>& Records = Harness.Records();
            REQUIRE(Records.back().Stamp.Tick > Before);
            REQUIRE(Records.back().Stamp.Window.EndNs - Records.back().Stamp.Window.StartNs == SimRate::StepNs(60));
        }

        // While a replay plays: refused, and every tick keeps the recording's rate
        {
            HarnessOptions Options = InMode(Mode);
            Options.Replay = Recording;
            ThreadedWorldHarness Harness(Options);
            RunSettings Settings;
            RunControls Controls(Settings, Harness.GetWorld());
            Harness.StartPlay();
            Harness.StepTicks(3);
            REQUIRE_FALSE(Controls.RequestSimHz(60, Error));
            REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("replay is playing"));
            Harness.StepTicks(5);
            Harness.StopPlay();
            REQUIRE(Harness.GetWorld().GetSimHz() == 30);
            REQUIRE(Harness.Records().size() == 8);
            for (const TickRecord& Record : Harness.Records())
            {
                REQUIRE(Record.Stamp.Window.EndNs - Record.Stamp.Window.StartNs == SimRate::StepNs(30));
            }
        }
    }
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: a rate change accepted before a recording or replay starts is dropped when it is applied",
          "[threads]")
{
    // Mutation note: fails (the rate becomes 60 Hz under the recording, and the file holds
    // cuts of two rates) if World::ApplyClockCommands does not check InputLocksSimRate.
    const TestScopes::CountLaneViolations Violations;
    const LaneCheck::Scope MainLane(Lane::Frame);
    const String RecordPath =
        (std::filesystem::temp_directory_path() / "fling-rate-change-window.input.jsonl").string();
    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        CAPTURE(ExecModeName(Mode));
        ThreadedWorldHarness Harness(InMode(Mode));
        RunSettings Settings;
        RunControls Controls(Settings, Harness.GetWorld());
        InputRecorder& Recorder = Input::Get().GetRecorder();
        Harness.StartPlay();
        Harness.StepTicks(3);

        // Accepted: nothing records yet. Then a recording starts before the next wake applies it
        String Error;
        REQUIRE(Controls.RequestSimHz(60, Error));
        Recorder.Start(RecordPath, 0.0f, 0.0f);
        Harness.StepTicks(4);
        Controls.OnFrame(PlatformTime::NowNanoseconds());
        REQUIRE(Harness.GetWorld().GetSimHz() == 30);

        // The recorder keeps its rate while it records
        REQUIRE_FALSE(Recorder.SetTickRate(60));
        REQUIRE(Recorder.GetTickRate() == 30);
        REQUIRE(Recorder.Stop());
        Harness.StopPlay();
        for (const TickRecord& Record : Harness.Records())
        {
            REQUIRE(Record.Stamp.Window.EndNs - Record.Stamp.Window.StartNs == SimRate::StepNs(30));
        }
        InputRecording Recording;
        REQUIRE(LoadInputRecording(RecordPath, Recording, Error));
        REQUIRE(Recording.TickCutRate == 30);
        REQUIRE(Recording.TickCuts.size() == 4);
    }
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
}

TEST_CASE("Exec modes: a Quit from a tick ends the session at that tick in every mode", "[threads]")
{
    for (const ExecMode Mode : {ExecMode::Direct, ExecMode::Inline, ExecMode::Threaded})
    {
        for (const bool bEditorPlay : {false, true})
        {
            CAPTURE(ExecModeName(Mode), bEditorPlay);
            const LaneCheck::Scope MainLane(Lane::Frame);
            ThreadedWorldHarness Harness(InMode(Mode));
            World& TheWorld = Harness.GetWorld();
            TheWorld.SetGameQuitStopsPlay(bEditorPlay);
            GetTickLog().QuitAtTick = 5;
            Harness.StartPlay();

            // Ten ticks are due; the session ends at the Quit tick
            Harness.StepTicks(10);
            REQUIRE(TheWorld.GetTickCount() == 5);
            REQUIRE(TheWorld.HasRunLastTick());
            if (bEditorPlay)
            {
                // Editor play stops at a frame that applies the Quit (the same one on this thread)
                for (int Frame = 0; Frame < 2 && TheWorld.IsPlaying(); ++Frame)
                {
                    Harness.StepFrame();
                }
                REQUIRE_FALSE(TheWorld.IsPlaying());
                REQUIRE_FALSE(TheWorld.ShouldQuit());
                REQUIRE(TheWorld.GetGameLane() == nullptr);
                REQUIRE(Harness.GetGame().Stops == 1);
            }
            else
            {
                Harness.StepFrame();
                REQUIRE(TheWorld.ShouldQuit());
                REQUIRE(TheWorld.IsPlaying());
                REQUIRE(TheWorld.GetTickCount() == 5);
            }
            // No play tick ran after the Quit tick: its slot of the tick log is as it was made
            REQUIRE_FALSE(SameBits(GetTickLog().Seen[5].Camera, TestCamera{}));
            REQUIRE(SameBits(GetTickLog().Seen[6].Camera, TestCamera{}));
        }
    }
}

TEST_CASE("Exec modes: a shutdown during threaded play joins the game thread and stops the game once", "[threads]")
{
    const LaneCheck::Scope MainLane(Lane::Frame);
    HarnessOptions Options = InMode(ExecMode::Threaded);
    Options.Pacing = GameLanePacing::AsFastAsPossible;
    ThreadedWorldHarness Harness(Options);
    World& TheWorld = Harness.GetWorld();
    Harness.StartPlay();
    for (int i = 0; i < 50; ++i)
    {
        Harness.AdvanceClockAhead(4);
        Harness.StepFrame();
    }

    // As a window close does: Shutdown with the game thread still ticking
    TheWorld.Shutdown();
    REQUIRE(TheWorld.GetGameLane() == nullptr);
    REQUIRE_FALSE(TheWorld.IsPlaying());
    REQUIRE(Harness.GetGame().Stops == 1);
    // A second Shutdown (the harness's) does nothing
    TheWorld.Shutdown();
    REQUIRE(Harness.GetGame().Stops == 1);
}

TEST_CASE("Exec modes: the lane checks see the main thread touch the game thread's storage", "[threads]")
{
#if FLING_LANE_CHECKS
    const TestScopes::CountLaneViolations Violations;
    const LaneCheck::Scope MainLane(Lane::Frame);
    ThreadedWorldHarness Harness(InMode(ExecMode::Threaded));
    Harness.StartPlay();
    Harness.StepTicks(2);

    // The mirror is the main thread's; the simulation storage is the game thread's. Every
    // entry point checks, the reads too.
    EntityStorage& Mirror = Harness.GetWorld().GetPresentedStorage();
    (void)Mirror.Count<TestCamera>();
    (void)Mirror.EntityCount();
    REQUIRE(TestScopes::CountLaneViolations::Count == 0);
    EntityStorage& Simulation = Harness.GetWorld().GetStorage();
    const std::vector<std::pair<const char*, std::function<void()>>> Reads = {
        {"TryGetSingleton",
         [&]
         {
             (void)Simulation.TryGetSingleton<SimTime>();
         }},
        {"Count",
         [&]
         {
             (void)Simulation.Count<TestCamera>();
         }},
        {"EntityCount",
         [&]
         {
             (void)Simulation.EntityCount();
         }},
        {"IsValid",
         [&]
         {
             (void)Simulation.IsValid(Entity::Null);
         }},
        {"AllEntities",
         [&]
         {
             (void)Simulation.Native().AllEntities();
         }},
        {"Archetypes",
         [&]
         {
             (void)Simulation.Native().Archetypes();
         }},
    };
    for (const auto& [Name, Read] : Reads)
    {
        CAPTURE(Name);
        TestScopes::CountLaneViolations::Count = 0;
        Read();
        REQUIRE(TestScopes::CountLaneViolations::Count > 0);
    }
    Harness.StopPlay();
#else
    SUCCEED("Lane checks are compiled out in this build");
#endif
}
