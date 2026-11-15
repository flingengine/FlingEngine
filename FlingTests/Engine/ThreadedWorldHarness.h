#pragma once

// A world with the test camera (TestSystems/TestCamera.h), driven tick by tick on a virtual clock, in any exec mode
// (Docs/threading-model.md section 9). The calling thread (the main thread, Lane::Frame) is
// the input writer in every mode, as in the engine: it moves the clock, then queues the
// script input up to the clock and calls Input::Poll. The input path is the engine's: the
// Input singleton (playback, device interfaces, recorder), the engine's tick input rule
// (TickInputProvider with World::MakeTickInputConfig) for the ticks, and PendingInputReader
// plus Input::PollLate for the frames. In Threaded mode the game thread cuts the queue that
// this thread writes. A live run can record (InputRecorder), and a replay plays a recording
// (InputPlayback), so a replay takes exactly the recorded cuts through the same code as
// Sandbox --replay-input. With Stepped pacing StepTicks moves the clock k ticks and lets the
// game thread run one wake; StepFrame runs the frame side on this thread against the newest
// packet.

#include "Components/AppRequests.h"
#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Publish/PublishChannel.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "ExecMode.h"
#include "Game.h"
#include "GameLane.h"
#include "Input/Input.h"
#include "Input/InputRecording.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Logger.h"
#include "PlatformTime.h"
#include "SimRate.h"
#include "TestSystems/TestCamera.h"
#include "World.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <memory>
#include <optional>
#include <vector>

namespace ThreadedTest
{
    using namespace Fling;
    using TestLook::SameBits;
    using TestLook::TestCamera;

    /** One row of the tick ledger that must not depend on the exec mode or the frame rate. */
    struct Row
    {
        uint64 Tick = 0;
        uint64 LastSequence = 0;
        uint32 MouseSamples = 0;
        uint32 KeyEvents = 0;
        uint64 StateHash = 0;

        bool operator==(const Row&) const = default;
    };

    /**
     * True if two ledgers gave every tick the same input and state: the tick, its counts and
     * its state hash. A replay's sequences are not the recording's (its messages come through
     * the playback devices), so a replay is compared with its recording by this.
     */
    inline bool SameInputAndState(const std::vector<Row>& t_A, const std::vector<Row>& t_B)
    {
        if (t_A.size() != t_B.size())
        {
            return false;
        }
        for (size_t i = 0; i < t_A.size(); ++i)
        {
            if (t_A[i].Tick != t_B[i].Tick || t_A[i].MouseSamples != t_B[i].MouseSamples ||
                t_A[i].KeyEvents != t_B[i].KeyEvents || t_A[i].StateHash != t_B[i].StateHash)
            {
                return false;
            }
        }
        return true;
    }

    /** A seeded script of look input: mouse moves and presses of F, Equal and Minus, at times that never repeat. */
    struct ScriptEvent
    {
        int64 TimeNs = 0;
        bool Mouse = true;
        int32 Dx = 0;
        int32 Dy = 0;
        InputKey Key;
        bool Down = false;
    };

    inline std::vector<ScriptEvent> MakeScript(uint64 t_Seed, const int t_Count)
    {
        const auto Next = [&t_Seed]()
        {
            t_Seed = t_Seed * 6364136223846793005ull + 1442695040888963407ull;
            return static_cast<uint32>(t_Seed >> 33);
        };
        const auto Range = [&Next](const int32 t_Min, const int32 t_Max)
        {
            return t_Min + static_cast<int32>(Next() % static_cast<uint32>(t_Max - t_Min + 1));
        };
        const InputKey PolicyKeys[] = {Keys::F, Keys::Equal, Keys::Minus};
        std::vector<ScriptEvent> Script;
        int64 Time = 1'000'000;
        for (int i = 0; i < t_Count; ++i)
        {
            Time += Range(1, 9) * 1'000'000 + Range(1, 999) * 1000;
            if (Range(0, 9) == 0)
            {
                const InputKey Key = PolicyKeys[Range(0, 2)];
                Script.push_back(ScriptEvent{.TimeNs = Time, .Mouse = false, .Key = Key, .Down = true});
                Time += Range(1, 30) * 100'000;
                Script.push_back(ScriptEvent{.TimeNs = Time, .Mouse = false, .Key = Key, .Down = false});
            }
            else
            {
                Script.push_back(ScriptEvent{.TimeNs = Time, .Dx = Range(-40, 40), .Dy = Range(-60, 60)});
            }
        }
        return Script;
    }

    /** What the simulation had at the end of one tick, before its packet was filled. */
    struct TickSeen
    {
        uint64 Cut = 0;
        TestCamera Camera;
        /** SimTime as the tick's systems saw it. */
        SimTime Time;
    };

    /**
     * Filled by the test systems on the thread that runs the ticks; systems have no state,
     * so it is global. One slot per tick, sized before play: the tick thread writes slot t
     * before it publishes the packet of tick t, and the frame side reads slot t only after
     * it applied that packet (the mailbox's release and acquire), so it needs no lock.
     */
    struct TickLog
    {
        std::vector<TickSeen> Seen;

        /** QuitSystem asks to quit on this tick. 0: never. Set before play. */
        uint64 QuitAtTick = 0;

        /** How many TestCamera entities the frame side's OnPlayBegin saw (FramePlayBeginProbe). */
        std::atomic<int> CamerasAtFramePlayBegin{-1};
    };

    inline TickLog& GetTickLog()
    {
        static TickLog s_Log;
        return s_Log;
    }

    /** Records the cut and the camera of each tick, at the end of the tick. */
    class TickRecorderSystem final
        : public System<TickRecorderSystem, Access<Read<SimTime>, Read<InputState>, Read<TestCamera>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "ThreadedTest.TickRecorder", .In = Order::Finalize};

        void FixedUpdate(Context& ctx) const
        {
            TickSeen Seen;
            Seen.Cut = ctx.Singleton<const InputState>().Tick.Slice.Cut.LastSequence;
            Seen.Time = ctx.Singleton<const SimTime>();
            for (auto [entity, camera] : ctx.Query<const TestCamera>())
            {
                (void)entity;
                Seen.Camera = camera;
            }
            std::vector<TickSeen>& Slots = GetTickLog().Seen;
            const uint64 Tick = ctx.Singleton<const SimTime>().Tick;
            if (Tick < Slots.size())
            {
                Slots[Tick] = Seen;
            }
        }
    };

    /** Asks the application to quit on TickLog::QuitAtTick. */
    class QuitSystem final : public System<QuitSystem, Access<Read<SimTime>, Append<AppRequests>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "ThreadedTest.Quit", .In = Order::Input};

        void FixedUpdate(Context& ctx) const
        {
            if (ctx.Singleton<const SimTime>().Tick == GetTickLog().QuitAtTick)
            {
                ctx.Queue<AppRequests>().Push(AppRequest::Quit);
            }
        }
    };

    /** A frame system whose OnPlayBegin counts the cameras it sees: it must see the state play starts with. */
    class FramePlayBeginProbe final : public System<FramePlayBeginProbe, Access<Read<TestCamera>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "ThreadedTest.FramePlayBeginProbe", .In = Order::Presentation};

        void OnPlayBegin(Context& ctx) const
        {
            int Cameras = 0;
            for (auto [entity, camera] : ctx.Query<const TestCamera>())
            {
                (void)entity;
                (void)camera;
                ++Cameras;
            }
            GetTickLog().CamerasAtFramePlayBegin = Cameras;
        }

        void Update(Context&) const {}
    };

    /** The engine modules, the test camera, and the test systems. Play makes one camera. */
    class HarnessGame final : public Game
    {
    public:
        void Init(EntityStorage&) override {}

        void Register(ModuleRegistration& t_Modules) override
        {
            RegisterEngine(t_Modules);
            TestLook::RegisterCameraType(t_Modules);
            t_Modules.System<TestLook::CameraSystem>();
            t_Modules.System<TestLook::CameraViewSystem>();
            t_Modules.System<TickRecorderSystem>();
            t_Modules.System<QuitSystem>();
            t_Modules.System<FramePlayBeginProbe>();
        }

        void OnStartGame(EntityStorage& t_Reg) override
        {
            t_Reg.Create(TestCamera{});
            ++Starts;
        }

        void OnStopGame(EntityStorage&) override { ++Stops; }

        void Shutdown(EntityStorage&) override {}

        int Starts = 0;
        int Stops = 0;
    };

    struct HarnessOptions
    {
        ExecMode Mode = ExecMode::Direct;
        GameLanePacing Pacing = GameLanePacing::Stepped;
        /** The simulation rate: the world's SystemRunnerConfig::SimHz, and the clock step of a frame. */
        uint32 SimHz = SimRate::DefaultHz;
        /** World::SetLastTick. 0: no limit. */
        uint64 LastTick = 0;
        uint32 JitterMaxMicroseconds = 0;
        uint64 ScriptSeed = 1;
        int ScriptEvents = 600;
        /** The most ticks a run has: the size of the per-tick log. */
        uint64 MaxTicks = 40000;

        /**
         * World::MakeTickInputConfig in Threaded mode: how long a timed tick on the game
         * thread waits for this thread's watermark to reach the end of its window (the live
         * input rule): -1 with no limit (headless, a replay), 0 never (live windowed play). A
         * tick on this thread never waits: the input up to the clock is polled before it.
         */
        int64 WatermarkWaitNs = -1;

        /** Not empty: record play to this file (InputRecorder, from StartPlay to StopPlay). */
        String RecordTo;

        /** Set: a replay. Play plays it (InputPlayback), and the script is not sent. */
        std::optional<InputRecording> Replay;
    };

    class ThreadedWorldHarness
    {
    public:
        explicit ThreadedWorldHarness(const HarnessOptions& t_Options)
            : m_Options(t_Options),
              m_StepNs(SimRate::StepNs(t_Options.SimHz)),
              m_Script(MakeScript(t_Options.ScriptSeed, t_Options.ScriptEvents))
        {
            Logger::Get().Init();
            Logger::GetCurrentConsole()->set_level(spdlog::level::off);
            Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
            TickLog& Log = GetTickLog();
            Log.Seen.assign(static_cast<size_t>(t_Options.MaxTicks + 1), TickSeen{});
            Log.QuitAtTick = 0;
            Log.CamerasAtFramePlayBegin = -1;

            // The engine's input: the Input singleton, written by this thread only
            Input::Get().SetWriterLane(Lane::None);
            Input::Get().Init();
            const InputInterfaceId Id = Input::Get().AddInterface<VirtualInputDeviceInterface>();
            m_Virtual = Input::Get().FindInterface<VirtualInputDeviceInterface>(Id);
            InputDeviceInfo MouseInfo;
            MouseInfo.Type = InputDeviceType::Mouse;
            MouseInfo.HardwarePath = "virtual:mouse";
            MouseInfo.Capabilities = InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::RelativeMotion;
            m_Mouse = m_Virtual->CreateDevice(MouseInfo);
            InputDeviceInfo KeyboardInfo;
            KeyboardInfo.Type = InputDeviceType::Keyboard;
            KeyboardInfo.HardwarePath = "virtual:keyboard";
            KeyboardInfo.Capabilities = InputDeviceCapabilities::Keys;
            m_Keyboard = m_Virtual->CreateDevice(KeyboardInfo);
            Input::Get().Poll(0.0f);

            m_Game = std::make_shared<HarnessGame>();
            m_World = std::make_shared<World>(m_Game);
            m_World->SetExecMode(t_Options.Mode);
            m_World->SetSystemRunnerConfig(SystemRunnerConfig{.SimHz = t_Options.SimHz});
            Input::Get().GetRecorder().SetTickRate(t_Options.SimHz);
            Input::Get().GetPlayback().SetTickRate(t_Options.SimHz);
            m_World->SetTickClock([]() { return PlatformTime::NowNanoseconds(); });
            m_World->SetTickTimelineStart(0);
            m_World->SetLastTick(t_Options.LastTick);
            m_World->KeepTickRecords();

            // As Engine::Startup: the engine's tick input rule for this exec mode, the pending
            // input reader and the late poll. Threaded play checks that only this thread writes.
            if (t_Options.Mode == ExecMode::Threaded)
            {
                Input::Get().SetWriterLane(Lane::Frame);
            }
            auto TickInputs = std::make_shared<TickInputProvider>(
                Input::Get(), m_World->MakeTickInputConfig(t_Options.WatermarkWaitNs));
            m_World->SetTickInputSource([TickInputs](TickInput& t_Out, const TickWindow& t_Window)
                                        { TickInputs->Build(t_Out, t_Window); });
            auto Pending = std::make_shared<PendingInputReader>(Input::Get().GetHandler());
            m_World->SetPendingInputSource([Pending](const uint64 t_After, InputSlice& t_Out)
                                           { Pending->Read(t_After, t_Out); });
            m_World->SetLateInputPoll([]() { Input::Get().PollLate(); });

            GameLaneDesc Desc;
            Desc.Pacing = t_Options.Pacing;
            Desc.JitterSeed = static_cast<uint32>(t_Options.ScriptSeed);
            Desc.JitterMaxMicroseconds = t_Options.JitterMaxMicroseconds;
            m_World->SetGameLaneDesc(Desc);
            m_World->SetFrameJitter(
                ThreadJitter(static_cast<uint32>(t_Options.ScriptSeed + 1), t_Options.JitterMaxMicroseconds));

            REQUIRE(m_World->Init());
        }

        ~ThreadedWorldHarness()
        {
            m_World->Shutdown();
            Input::Get().GetPlayback().Stop();
            Input::Get().GetRecorder().Stop();
            Input::Get().Shutdown();
            Input::Get().SetWriterLane(Lane::None);
        }

        ThreadedWorldHarness(const ThreadedWorldHarness&) = delete;
        ThreadedWorldHarness& operator=(const ThreadedWorldHarness&) = delete;

        /**
         * Start play: the next frame runs StartGame, and in Threaded mode starts the game
         * thread. A replay starts its playback first, a recording its recorder.
         */
        void StartPlay()
        {
            if (m_Options.Replay)
            {
                String StartError;
                REQUIRE(Input::Get().GetPlayback().Start(*m_Options.Replay, 0.0f, StartError));
                REQUIRE(Input::Get().GetPlayback().HasTickCuts());
            }
            if (!m_Options.RecordTo.empty())
            {
                Input::Get().GetRecorder().Start(m_Options.RecordTo, 0.0f, 0.0f);
            }
            m_World->RequestGameStart();
            m_World->Update(0.0f);
            REQUIRE(m_World->IsPlaying());
        }

        void StopPlay()
        {
            m_World->RequestGameStop();
            m_World->Update(0.0f);
            REQUIRE_FALSE(m_World->IsPlaying());
            CollectNewRows();
            // The game thread has joined: every tick's cut is noted
            if (!m_Options.RecordTo.empty())
            {
                REQUIRE(Input::Get().GetRecorder().Stop());
            }
        }

        /** The recording of the last play (RecordTo), loaded from its file. */
        InputRecording LoadRecording() const
        {
            InputRecording Recording;
            String Error;
            const bool Loaded = LoadInputRecording(m_Options.RecordTo, Recording, Error);
            INFO(Error);
            REQUIRE(Loaded);
            return Recording;
        }

        /**
         * Move the clock by t_Ticks ticks (polling the input after each step, as a frame does),
         * then run one wake: on the game thread (Threaded play), or as one engine frame on this
         * thread. More than SystemRunnerConfig::MaxTicksPerWake ticks is a hitch: the rest of the time is dropped.
         */
        void StepTicks(const uint32 t_Ticks)
        {
            for (uint32 i = 0; i < t_Ticks; ++i)
            {
                AdvanceClock();
            }
            if (m_World->GetGameLane() != nullptr)
            {
                m_World->StepGameLane(1);
            }
            else
            {
                m_World->Update(StepSeconds());
            }
            CollectNewRows();
        }

        /**
         * Move the clock by t_Ns (polling the input, as a frame does), then run one wake: on the
         * game thread (Threaded play), or as one engine frame on this thread.
         */
        void StepTime(const int64 t_Ns)
        {
            AdvanceClockBy(t_Ns);
            if (m_World->GetGameLane() != nullptr)
            {
                m_World->StepGameLane(1);
            }
            else
            {
                m_World->Update(static_cast<float>(t_Ns) / 1e9f);
            }
            CollectNewRows();
        }

        /**
         * A free game thread's frame: move the clock one tick and poll, but never more than
         * t_MaxAhead ticks ahead of the ticks that ran (the headless rule that keeps the hitch
         * rule from firing on a clock that never stalls).
         */
        void AdvanceClockAhead(const uint64 t_MaxAhead)
        {
            const uint64 Due = static_cast<uint64>(m_Clock.Clock().Now() / m_StepNs);
            if (m_World->HasRunLastTick() || m_World->GetTickCount() + t_MaxAhead >= Due)
            {
                AdvanceClock();
            }
        }

        /** Run the frame side once at the current clock (no tick is due), then take the new ledger rows. */
        void StepFrame()
        {
            m_World->Update(0.001f);
            CollectNewRows();
        }

        /**
         * The frame side never sees a torn state: the tick, the cut, the camera and the
         * view in the frame storage all belong to the one tick the frame shows.
         */
        void CheckShownState()
        {
            EntityStorage& Shown = m_World->GetPresentedStorage();
            const uint64 Tick = Shown.Singleton<RealFrameTime>().Latest.Tick;
            const uint64 Cut = Shown.Singleton<RealFrameTime>().Latest.Cut.LastSequence;
            REQUIRE(Tick == m_World->GetShownTick());
            if (const PublishChannel* Channel = m_World->GetPublishChannel())
            {
                REQUIRE(Channel->Shown().Stamp.Tick == Tick);
                REQUIRE(Channel->Shown().Stamp.Cut.LastSequence == Cut);
            }
            if (Tick == 0)
            {
                return;
            }
            REQUIRE(Tick < GetTickLog().Seen.size());
            const TickSeen Expected = GetTickLog().Seen[Tick];
            REQUIRE(Cut == Expected.Cut);
            int Cameras = 0;
            Shown.View<TestCamera>().Each(
                [&](const TestCamera& t_Camera)
                {
                    ++Cameras;
                    REQUIRE(SameBits(t_Camera, Expected.Camera));
                    // The late look of this frame starts after the shown tick's cut
                    REQUIRE(Shown.Singleton<PendingInput>().Cut.AfterSequence == Cut);
                });
            REQUIRE(Cameras == 1);
            ++m_CheckedFrames;
        }

        /** The ledger rows of every tick so far, in order. */
        std::vector<Row> Rows() const { return m_Rows; }

        /** The whole ledger records of every tick so far, in order (with the wall-clock windows). */
        const std::vector<TickRecord>& Records() const { return m_Records; }

        World& GetWorld() { return *m_World; }

        HarnessGame& GetGame() { return *m_Game; }

        uint64 CheckedFrames() const { return m_CheckedFrames; }

    private:
        /**
         * This thread is the writer, as the engine's frame: the clock moves one tick, the
         * script input up to it is queued (not in a replay), then Input::Poll runs the
         * playback, polls the interfaces and lets the recorder read.
         */
        void AdvanceClock() { AdvanceClockBy(m_StepNs); }

        /** As AdvanceClock, by t_Ns. */
        void AdvanceClockBy(const int64 t_Ns)
        {
            const int64 Now = m_Clock.Clock().Now() + t_Ns;
            m_Clock.Clock().Set(Now);
            while (!m_Options.Replay && m_NextEvent < m_Script.size() && m_Script[m_NextEvent].TimeNs <= Now)
            {
                const ScriptEvent& Event = m_Script[m_NextEvent];
                InputMessage Message;
                Message.TimeNs = Event.TimeNs;
                if (Event.Mouse)
                {
                    Message.Device = m_Mouse;
                    Message.Type = InputMessageType::MouseMove;
                    Message.DeltaX = Event.Dx;
                    Message.DeltaY = Event.Dy;
                }
                else
                {
                    Message.Device = m_Keyboard;
                    Message.Type = Event.Down ? InputMessageType::KeyDown : InputMessageType::KeyUp;
                    Message.Key = Event.Key;
                }
                ++m_NextEvent;
                m_Virtual->Queue(Message, m_NextEvent);
            }
            Input::Get().Poll(static_cast<float>(t_Ns) / 1e9f);
        }

        /** One tick length in seconds, for the frame delta. */
        float StepSeconds() const { return static_cast<float>(m_StepNs) / 1e9f; }

        /** On this thread: the ledger rows of the ticks since the last call. */
        void CollectNewRows()
        {
            for (const TickRecord& R : m_World->TakeNewTickRecords())
            {
                m_Records.push_back(R);
                m_Rows.push_back(
                    Row{R.Stamp.Tick, R.Stamp.Cut.LastSequence, R.MouseSamples, R.KeyEvents, R.Stamp.StateHash});
            }
        }

        HarnessOptions m_Options;

        /** One tick at m_Options.SimHz, in nanoseconds: the clock step of a frame. */
        int64 m_StepNs = 0;

        PlatformTime::ScopedVirtualClock m_Clock{0};

        /** The script's devices, on the Input singleton. Written only by this thread (the writer). */
        std::shared_ptr<VirtualInputDeviceInterface> m_Virtual;
        InputDeviceId m_Mouse;
        InputDeviceId m_Keyboard;

        std::vector<ScriptEvent> m_Script;
        size_t m_NextEvent = 0;

        std::shared_ptr<HarnessGame> m_Game;
        std::shared_ptr<World> m_World;

        // This thread only
        std::vector<Row> m_Rows;
        std::vector<TickRecord> m_Records;
        uint64 m_CheckedFrames = 0;
    };
} // namespace ThreadedTest
