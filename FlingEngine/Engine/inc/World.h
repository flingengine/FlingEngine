#pragma once

#include "Components/AppRequests.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/Publish/PublishedSet.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Ecs/TypeRegistry.h"
#include "ExecMode.h"
#include "FlingTypes.h"
#include "Game.h"
#include "GameLane.h"
#include "Input/TickInputProvider.h"
#include "Json.h"
#include "Level.h"
#include "Platform.h"
#include "Strings/String.h"
#include "Threading/ThreadJitter.h"
#include "Threading/ThreadQueue.h"
#include "TickDriver.h"
#include "TickEvents.h"

#include <atomic>
#include <cassert>
#include <chrono>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Fling
{
    class PresentationLink;
    class PublishChannel;

    /**
     * The world holds all active levels in the game. There will only ever be exactly one World
     * instance at any given time.
     *
     * Threads: every public function is for the main thread (Lane::Frame), except
     * GetTickCount, HasRunLastTick, GetSimHz and ShouldTickStopWaiting. In Threaded play
     * the game thread runs the ticks (RunWake). What the two threads share (the published
     * packets, the outbox, the clock commands, the input queue and its replay and recorder
     * cuts, the tick clock, and the atomics behind those four functions) is listed in
     * Docs/threading-model.md section 8.
     * @see Level
     */
    class World final : private TickHost
    {
    public:
        World(const World&) = delete;
        World& operator=(const World&) = delete;

        /** The world shares the game, so the game lives as long as the world calls it. */
        explicit World(std::shared_ptr<Fling::Game> t_Game);

        /** Stops the game thread first, if it still runs. */
        ~World();

        /**
         * What the modules add to this world: component types and systems. Call each
         * module's register function with it before Init. Init then calls Game::Register,
         * builds the TypeRegistry, and builds the schedule.
         */
        ModuleRegistration& GetModules() { return m_Modules; }

        /** The ECS systems this world runs. Modules add them through GetModules. */
        SystemRegistry& GetSystems() { return m_Systems; }

        /** Every component type this world knows. Empty before Init. */
        const TypeRegistry& GetTypes() const { return m_Types; }

        /** How the fixed tick runs (step length, one tick per frame). Set it before Init. */
        void SetSystemRunnerConfig(const SystemRunnerConfig& t_Config) { m_RunnerConfig = t_Config; }

        const SystemRunnerConfig& GetSystemRunnerConfig() const { return m_RunnerConfig; }

        /**
         * How play runs (ExecMode): Direct, Inline (frame systems on a published copy) or
         * Threaded (ticks on a game thread). Edit mode is always Direct. Set it before Init.
         * The default is Direct, so a tool or a test that makes a World gets no thread it did
         * not ask for; the engine sets the run's mode (RunSettings, threaded by default).
         */
        void SetExecMode(const ExecMode t_Mode) { m_ExecMode = t_Mode; }

        ExecMode GetExecMode() const { return m_ExecMode; }

        /** How the game thread runs in Threaded play (pacing, jitter). Set it before play starts. */
        void SetGameLaneDesc(const GameLaneDesc& t_Desc) { m_GameLaneDesc = t_Desc; }

        /** --thread-jitter on the frame side: a sleep before each packet apply and before the frame systems. */
        void SetFrameJitter(const ThreadJitter& t_Jitter) { m_FrameJitter = t_Jitter; }

        /**
         * Set the function that fills each fixed tick's input (see SystemRunner). Set it
         * before Init. With none (for example in tests), ticks have no input.
         */
        void SetTickInputSource(TickInputSource t_Source) { m_TickInputSource = std::move(t_Source); }

        /**
         * Set the clock of the ticks and of the frames (PlatformTime::NowNanoseconds in the
         * engine). The ticks that are due and the frame time come from it, in every exec
         * mode and on every thread, so it must be thread safe. With none, the world sums the
         * deltas that Update gets, and the timeline starts at 0. Set it before Init.
         */
        void SetTickClock(std::function<int64()> t_Clock) { m_TickClock = std::move(t_Clock); }

        /**
         * Start the tick timeline at this clock time, instead of at the first Update. The
         * headless loop starts it one tick before its first frame's clock time, so tick 1
         * runs on frame 1. Set it before Init.
         */
        void SetTickTimelineStart(const int64 t_StartNs) { m_TickTimelineStart = t_StartNs; }

        /**
         * The tick clock only moves when frames run (the headless virtual clock): a wake that
         * is behind it was not stalled, so it never drops due time
         * (SystemRunnerConfig::MaxCatchUp). It runs at most MaxTicksPerWake ticks and the
         * game thread's next wake runs the rest, so the ticks and their windows do not depend
         * on how far the frames ran ahead. The frames never wait for the ticks. Set it before Init.
         */
        void SetTickClockOnlyMovesWithFrames(const bool t_Only) { m_TickClockOnlyMovesWithFrames = t_Only; }

        /**
         * Hash the simulation state after each tick, into the ledger (TickRecord::StateHash,
         * HashSimulationState). On by default. It walks every saved simulation component, so
         * it costs time in a big world. Set it before Init.
         */
        void SetStateHashEnabled(const bool t_Enabled) { m_StateHashEnabled = t_Enabled; }

        /**
         * The last tick that may run (--frames): no wake runs a tick after it, in any exec
         * mode. 0 (the default) has no limit. Set it before play starts.
         */
        void SetLastTick(const uint64 t_Tick) { m_LastTick = t_Tick; }

        /** Keep a copy of each tick's ledger record for TakeNewTickRecords (--dump-ticks, tests). Call it before Init.
         */
        void KeepTickRecords() { m_KeepTickRecords = true; }

        /** The ledger records of the ticks since the last call, in tick order (KeepTickRecords). */
        std::vector<TickRecord> TakeNewTickRecords();

        /**
         * Set the function that reads the input after a sequence, for the frame side
         * (see SystemRunner::SetPendingInputSource). Set it before Init.
         */
        void SetPendingInputSource(PendingInputSource t_Source) { m_PendingInputSource = std::move(t_Source); }

        /**
         * Set the function that Update calls after the fixed ticks, right before the variable
         * rate systems: the late input poll (Input::PollLate), so input that came in during
         * the ticks is pending for this frame's presentation. With none, nothing is called.
         */
        void SetLateInputPoll(std::function<void()> t_Poll) { m_LateInputPoll = std::move(t_Poll); }

        /** The schedule that Init built. Null before Init. */
        const Schedule* GetSchedule() const;

        /**
         * The AppRequests from the systems that the application handles (all but Quit,
         * which the world handles), in the order the ticks made them. Clears the list.
         */
        std::vector<AppRequest> TakeAppRequests() { return std::exchange(m_AppRequestsForApp, {}); }

        /** True during Inline or Threaded play: the frame side reads a mirror (GetPresentedStorage). */
        bool IsPresentingCopy() const { return GetPublishChannel() != nullptr; }

        /**
         * The storage the frame side reads: the mirror during Inline or Threaded play, the
         * simulation storage otherwise. The renderer and the editor read this one.
         */
        EntityStorage& GetPresentedStorage();

        /** The tick the frame side shows: the newest packet's in Inline and Threaded play, the newest tick otherwise.
         */
        uint64 GetShownTick() const;

        /** The packet path during Inline or Threaded play, null otherwise. */
        const PublishChannel* GetPublishChannel() const;

        /** The game thread during Threaded play, null otherwise. */
        const GameLane* GetGameLane() const { return m_GameLane.load(std::memory_order_acquire); }

        /** Stepped pacing (tests): let the game thread run t_Wakes wakes, and wait for them. Threaded play only. */
        void StepGameLane(uint32 t_Wakes);

        /**
         * Any thread: true when a tick that waits for the input writer (the main thread) must
         * stop waiting, because the main thread waits for the game thread to stop (or there is
         * no game thread). The tick input source of Threaded play asks it.
         */
        bool ShouldTickStopWaiting() const;

        /**
         * The tick input rule's config for this world's exec mode (TickInputProvider); the
         * engine and the threaded tests both use it. Direct and Inline run the ticks on the
         * writer's thread, which sends the replay cuts itself. Threaded: the game thread waits
         * for the main thread to send each replay cut, and a timed tick waits up to
         * t_ThreadedWatermarkWaitNs for the main thread's watermark to reach the end of its
         * window: -1 with no limit (a headless run or a replay, so the cuts are Direct's), 0
         * never (live windowed play: a tick takes what the main thread has polled, and the
         * recorder notes that cut). Call it after SetExecMode.
         */
        TickInputProviderConfig MakeTickInputConfig(int64 t_ThreadedWatermarkWaitNs) const;

        /**
         * Ticks run since Init. Any thread. The tick thread updates it after each wake, after
         * the wake's publish, so a thread that sees tick n counted can show tick n.
         */
        uint64 GetTickCount() const { return m_TickCount.load(std::memory_order_acquire); }

        /** Any thread: true after the last tick of this play session ran (SetLastTick, or a Quit). */
        bool HasRunLastTick() const override { return m_RanLastTick.load(std::memory_order_acquire); }

        /** Pause the ticks at the next wake (a clock command), in every exec mode. */
        void PauseTicks() { SendClockCommand(ClockCommand{ClockCommandType::Pause}); }

        /** Resume the ticks at the next wake. */
        void ResumeTicks() { SendClockCommand(ClockCommand{ClockCommandType::Resume}); }

        /** Send a clock command to the tick thread. A stamped one (BeforeTick) runs right before that tick. */
        void SendClockCommand(const ClockCommand& t_Command);

        /**
         * Change the simulation rate (a SetRate clock command): the tick thread applies it
         * between two ticks, right before t_BeforeTick, or at the next wake when it is 0 (live
         * play). From that tick on the windows, SimTime::FixedDeltaNs and the physics step have
         * the new length; tick numbers go on by one.
         *
         * False, with the reason, for a rate that is not valid (SimRate::IsValidHz) or while an
         * input replay plays or a recording runs (each holds cuts at one rate). The rule is
         * checked again when the command is applied: a change that a replay or a recording
         * started after the request is dropped then, with a log line.
         */
        bool RequestSimHz(uint32 t_Hz, String& t_OutError, uint64 t_BeforeTick = 0);

        /** Any thread: the rate the ticks run at now. SystemRunnerConfig::SimHz until a SetRate command ran. */
        uint32 GetSimHz() const { return m_SimHz.load(std::memory_order_acquire); }

        /**
         * Draw between the latest two published ticks (on, the default) or the latest tick only
         * (FrameBlend::SetInterpolation). Presentation only: no tick reads it.
         */
        void SetInterpolation(bool t_On);

        /** @see SetInterpolation */
        bool GetInterpolation() const { return m_Blend.Interpolation(); }

        /** The schedule errors and component name errors from Init. Empty if both are valid. */
        const String& GetScheduleErrors() const { return m_ScheduleErrors; }

        /**
         * Initializes the world: registers components and systems, builds the system
         * schedule, calls each system's OnCreate, then loads the StartLevel that is
         * specified in the config.
         * @note		Keep explicit Init and Shutdown functions to make the startup order more readable
         *
         * @param t_StartLevel	A level to load instead of the config StartLevel (relative to the assets folder). Empty
         * means use the config.
         * @return False if the system schedule or the component names have errors (see GetScheduleErrors). The world is
         * then not ready for play.
         */
        bool Init(StringView t_StartLevel = {});

        /**
         * Run one frame: the pending play request, the wakes of this frame (with a
         * SameThreadDriver, the fixed ticks that are due; the game thread runs them itself in
         * Threaded play), the outbox of the ticks, then the frame side on the newest state the
         * link shows. Systems run in the Play role during play and the Edit role otherwise.
         *
         * @param t_DeltaTime   Time between previous frame and the current one.
         */
        void Update(float t_DeltaTime);

        /** Called just before destruction. Stops play first (the game thread joins). Runs once. */
        void Shutdown();

        /**
         * Ask the world to start play. The request runs at the start of the next Update,
         * not during the call. Before OnStartGame, the world keeps a snapshot of all
         * entities, so that RequestGameStop can put the world back.
         * Has no effect if the world is not ready for play.
         */
        void RequestGameStart();

        /**
         * Ask the world to stop play. The request runs at the start of the next Update,
         * not during the call. The world calls OnStopGame, then removes all entities and
         * loads the snapshot from RequestGameStart again.
         * Has no effect if the world is not playing.
         */
        void RequestGameStop();

        /**
         * Set what happens when a tick asks to quit (AppRequest::Quit) during play.
         * When true, play stops and the application keeps running (editor play).
         * When false (the default), the application quits. Set it before play starts.
         */
        void SetGameQuitStopsPlay(bool t_StopsPlay) { m_GameQuitStopsPlay = t_StopsPlay; }

        /**
         * Ask the engine to exit after this frame, even in the editor (unlike the Quit
         * AppRequest, which only stops play there).
         */
        void RequestQuit() { m_QuitRequested = true; }

        /**
         * Check if the world wants to exit the program: a Quit from a tick (outside editor
         * play), or RequestQuit.
         * @see Engine::RunFrames
         */
        bool ShouldQuit() const { return m_QuitRequested; }

        /**
         * Serialize all registered components on living entities to a JSON level file.
         * An edit: each entity that has no EntityGuid (or shares one) keeps the id that the
         * file gives it, so the next save writes the same ids.
         *
         * @param t_LevelToLoad File path to write (relative to the assets directory)
         * @return True on success
         */
        bool OutputLevelFile(StringView t_LevelToLoad);

        /**
         * Reset the current registry and load entities/components from a JSON level file.
         * Requires Init, which builds the type registry.
         *
         * @param t_LevelToLoad File path to load (relative to the assets directory)
         * @return True on success
         */
        bool LoadLevelFile(StringView t_LevelToLoad);

        /**
         * Write all saved components on living entities to a level document. It changes
         * nothing in the storage: an entity that has no EntityGuid (or shares one) gets a
         * new id in the document only, so references between entities save. OutputLevelFile
         * and the play snapshot use it.
         */
        bool SaveToJson(Json& t_OutDoc, StringView t_Title) const;

        /**
         * Remove all entities, then make the entities in a level document. A component
         * loads after the components that it requires, and one that the document does not
         * have is added with default values. LoadLevelFile and the play snapshot use it.
         */
        bool LoadFromJson(const Json& t_Doc, StringView t_SourceName);

        /**
         * The entities, components, and singletons of this world: the simulation storage.
         * During Inline and Threaded play it belongs to the tick thread (Lane::Game); the frame
         * side reads GetPresentedStorage instead.
         */
        FORCEINLINE EntityStorage& GetStorage() { return m_Storage; }

        FORCEINLINE const EntityStorage& GetStorage() const { return m_Storage; }

        // The current state of the game
        enum class WorldState : uint8
        {
            NONE,         // The game has not been initalized yet. Starting state.
            Initalized,   // True after the game has completed Initalization logic
            Starting,     // True while the game is in it's "OnStartGame" logic
            Playing,      // The game has started and is actively ticking
            ShuttingDown, // True when the game is in it's shutdown phase
        };

        inline bool IsPlaying() const { return m_CurrentState == WorldState::Playing; }
        inline bool IsReadyForPlay() const { return m_CurrentState == WorldState::Initalized; }

        /** True if a start or stop request waits for the next Update */
        inline bool HasPendingPlayRequest() const { return m_PendingRequest != PlayRequest::None; }

        inline WorldState GetState() const { return m_CurrentState; }

    private:
        enum class PlayRequest : uint8
        {
            None,
            Start,
            Stop,
        };

        /** What StopGame does with the entities. */
        enum class RestoreSnapshot : uint8
        {
            /** Put back the entities from play start (editor stop). */
            Yes,
            /** Keep the entities as play left them (shutdown). */
            No,
        };

        // TickHost, for the tick thread (the game thread, or this one with a SameThreadDriver)
        uint32 RunWake() override;
        int64 NanosecondsToNextTick() const override;
        bool IsTickClockPaused() const override;

        /** This world as the host of its drivers (a private base, so the conversion is made here). */
        TickHost& AsTickHost() { return *this; }

        /** A new EntityGuid for each entity that has none, or the same one as an entity before it. */
        std::unordered_map<Fling::Entity, uint64> MakeMissingEntityGuids() const;

        /** Run the pending start or stop request, if there is one */
        void ProcessPlayRequest();

        /** Keep a snapshot of the entities, begin play in the exec mode, then hand the ticks to its driver. */
        void StartGame();

        /** Join the tick thread, end play (OnPlayEnd, OnStopGame), and go back to Edit. */
        void StopGame(RestoreSnapshot t_Restore);

        /**
         * Make t_Link the frame side's link: the frame systems move to its frame storage, and
         * the blend starts again.
         */
        void UseLink(std::unique_ptr<PresentationLink> t_Link);

        /** Main thread: the link shows its newest tick, and the blend takes the latest two. Returns the newest. */
        TickStamp ShowLatest();

        /** The driver of the exec mode's play: false if the game thread cannot start. */
        bool StartPlayDriver();

        /** Back to the SameThreadDriver of Edit (joins the game thread). */
        void UseEditDriver();

        /** Tick thread, at the start of a wake: apply the clock commands whose tick has come. */
        void ApplyClockCommands(int64 t_NowNs);

        /**
         * Any thread: true, with the reason, while an input replay plays or a recording runs.
         * Each holds tick cuts at one rate, so the rate (now t_Hz) must not change.
         */
        static bool InputLocksSimRate(uint32 t_Hz, String& t_OutReason);

        /**
         * Tick thread: the time a wake runs its ticks to. The clock, but with a tick clock that only
         * moves with the frames at most MaxTicksPerWake ticks past the next window, so the hitch
         * rule never fires there and the next wake goes on.
         */
        int64 WakeTimeFrom(int64 t_ClockNs) const;

        /** Tick thread: the last tick of this session (SetLastTick, a Quit). The largest uint64 when there is none. */
        uint64 LastTickOfSession() const;

        /** Tick thread: how many ticks this wake may run (the last tick, a stamped clock command). */
        uint32 TickLimit() const;

        /**
         * Tick thread, after each tick (SystemRunner::SetTickOutput): publish it, then its
         * AppRequests into the outbox and its record to the feed. False: a Quit.
         */
        bool OnTickDone(uint64 t_Tick);

        /** Main thread: apply the outbox entries of the ticks that have not been applied. */
        void ApplyTickOutbox();

        /** Main thread: a tick asked to quit: stop play (editor) or quit the application. */
        void OnGameQuit();

        /** The frame side: the newest state, the late input poll, the variable rate groups. */
        void RunFrame(std::chrono::nanoseconds t_RealDelta);

        WorldState m_CurrentState = WorldState::NONE;

        PlayRequest m_PendingRequest = PlayRequest::None;

        /** The entities as they were when play started. Null when there is no snapshot to restore. */
        Json m_PlaySnapshot;

        /** @see SetGameQuitStopsPlay */
        bool m_GameQuitStopsPlay = false;

        /** All entities in this world. Declared before the systems and runner, so it is destroyed after them. */
        EntityStorage m_Storage;

        /** The systems this world runs. */
        SystemRegistry m_Systems;

        /** @see GetModules. Declared after m_Systems, which it refers to. */
        ModuleRegistration m_Modules{m_Systems};

        /** @see GetTypes. Built by Init. */
        TypeRegistry m_Types;

        /** True after Init built m_Types. A level cannot load or save before that. */
        bool m_TypesBuilt = false;

        /** @see SetSystemRunnerConfig */
        SystemRunnerConfig m_RunnerConfig;

        /** @see SetExecMode */
        ExecMode m_ExecMode = ExecMode::Direct;

        /** The types the tick thread copies to the frame side in Inline and Threaded play. Built and checked by Init in
         * every exec mode. */
        PublishedSet m_Published;

        /** @see SetGameLaneDesc */
        GameLaneDesc m_GameLaneDesc;

        /** @see SetFrameJitter */
        ThreadJitter m_FrameJitter;

        /** @see GetTickCount */
        std::atomic<uint64> m_TickCount{0};

        /** @see HasRunLastTick */
        std::atomic<bool> m_RanLastTick{false};

        /** @see GetSimHz. Written by the tick thread. */
        std::atomic<uint32> m_SimHz{0};

        /** The renderer's interpolation state (frame side only). @see SetInterpolation */
        FrameBlend m_Blend;

        /** @see SetTickInputSource */
        TickInputSource m_TickInputSource;

        /** @see SetTickClock. Always set after Init. */
        std::function<int64()> m_TickClock;

        /** With no tick clock given: the sum of the Update deltas, which the default tick clock reads. */
        std::atomic<int64> m_SummedDeltaNs{0};

        /** @see SetTickTimelineStart */
        std::optional<int64> m_TickTimelineStart;

        /** @see SetTickClockOnlyMovesWithFrames */
        bool m_TickClockOnlyMovesWithFrames = false;

        /** @see SetStateHashEnabled */
        bool m_StateHashEnabled = true;

        /** @see SetLastTick */
        uint64 m_LastTick = 0;

        /** @see KeepTickRecords */
        bool m_KeepTickRecords = false;

        /** @see SetPendingInputSource */
        PendingInputSource m_PendingInputSource;

        /** @see SetLateInputPoll */
        std::function<void()> m_LateInputPoll;

        /**
         * How the frame side sees the simulation. Made by Init; replaced only at play start and
         * stop. Declared before the runner, whose frame systems may live in its mirror.
         */
        std::unique_ptr<PresentationLink> m_Link;

        /** Runs the schedule. Made by Init when the schedule has no errors. */
        std::unique_ptr<SystemRunner> m_Runner;

        /** @see GetScheduleErrors */
        String m_ScheduleErrors;

        /** Which thread runs the wakes. Made by Init; replaced only at play start and stop. ~World destroys it first.
         */
        std::unique_ptr<TickDriver> m_Driver;

        /**
         * m_Driver when it is the game thread (Threaded play), null otherwise. Set before the
         * thread starts and cleared before it is asked to stop, so the game thread's input
         * waits can read it (null: stop waiting).
         */
        std::atomic<GameLane*> m_GameLane{nullptr};

        // The tick thread's state: the main thread writes it only while no tick thread runs

        /** The role of the ticks of this session (Play or Edit). */
        WorldRole m_TickRole = WorldRole::Edit;

        /** A Quit ran on this tick and ends the session there. 0: none. */
        uint64 m_QuitTick = 0;

        /** Stamped clock commands that wait for their tick, in the order they came. */
        std::vector<ClockCommand> m_WaitingClockCommands;

        // Between the threads

        /** Main thread to tick thread. */
        ThreadQueue<ClockCommand> m_ClockCommands;

        /** Tick thread to main thread: what each tick sent out. */
        ThreadQueue<TickOutboxEntry> m_Outbox;

        /** Tick thread to main thread: @see KeepTickRecords */
        ThreadQueue<TickRecord> m_NewTickRecords;

        // The main thread's state

        /** The newest tick whose outbox entry was applied. An entry that is not newer is a repeat. */
        uint64 m_LastAppliedOutboxTick = 0;

        /** @see TakeAppRequests */
        std::vector<AppRequest> m_AppRequestsForApp;

        /** @see ShouldQuit */
        bool m_QuitRequested = false;

        /** The game will allow users to specify their own update/read/write functions */
        std::shared_ptr<Fling::Game> m_Game;
    };

} // namespace Fling
