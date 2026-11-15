#pragma once

#include "Ecs/EntityStorage.h"
#include "Ecs/Order.h"
#include "Ecs/Schedule.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemContext.h"
#include "Ecs/TickLedger.h"
#include "FlingTypes.h"
#include "Input/InputSlice.h"
#include "Input/TickInput.h"
#include "SimRate.h"
#include "TickTimeline.h"

#include <chrono>
#include <functional>
#include <vector>

namespace Fling
{
    class DeferredCommands;
    class HeldValues;
    struct BlendFrame;
    class SystemRegistry;

    struct SystemRunnerConfig
    {
        /**
         * The fixed simulation rate in ticks a second (SimRate). It sets the length of every
         * tick: the timeline's windows and SimTime::FixedDeltaNs. SystemRunner::SetSimHz
         * changes it at a tick boundary.
         */
        uint32 SimHz = SimRate::DefaultHz;

        /** The length of one tick at SimHz. */
        std::chrono::nanoseconds FixedStep() const { return std::chrono::nanoseconds(SimRate::StepNs(SimHz)); }

        /**
         * The most clock time one wake may catch up. After a longer stall the rest of the due
         * time is dropped (TickTimeline::DropBacklog), not caught up, so a hitch does not
         * turn into a burst of ticks. Tick numbers are never skipped. It is a time, not a
         * tick count, so the hitch rule is the same at every SimHz.
         */
        static constexpr std::chrono::nanoseconds MaxCatchUp{250'000'000};

        /** The most ticks one wake may run: MaxCatchUp in ticks at SimHz, rounded up (8 at 30 Hz, 250 at 1000 Hz). */
        uint32 MaxTicksPerWake() const
        {
            const int64 step = SimRate::StepNs(SimHz);
            return static_cast<uint32>((MaxCatchUp.count() + step - 1) / step);
        }

        /** Run exactly one tick per frame, whatever the real time. For tests: the engine does not set it. */
        bool OneTickPerFrame = false;
        /**
         * With OneTickPerFrame: run the tick only on every Nth call of RunFixed (the first,
         * then N later, ...), so N - 1 frames run with no tick. 1 is a tick every frame.
         * A headless run uses it to have frames between ticks (--frames-per-tick).
         */
        uint32 FramesPerTick = 1;
    };

    /** Fills the input of one tick. The window says which messages belong to it (TickWindow::Trigger). */
    using TickInputSource = std::function<void(TickInput&, const TickWindow&)>;

    /** Fills an InputSlice with the input messages after a sequence, up to now. It consumes nothing. */
    using PendingInputSource = std::function<void(uint64 afterSequence, InputSlice& out)>;

    /** Hashes the simulation state after a tick, for the ledger (HashSimulationState). */
    using StateHasher = std::function<uint64(const EntityStorage&)>;

    /**
     * Takes what a tick sends out of the simulation (the world's outbox: AppRequests), after
     * the tick is in the ledger. It runs no gameplay and changes no hashed state. Returns
     * false to end the wake after this tick (a Quit).
     */
    using TickOutput = std::function<bool(uint64 tick)>;

    /**
     * Runs a schedule. The fixed groups run against the simulation storage. The variable
     * rate groups (the frame side) run against the frame storage: the same storage by
     * default (Direct), or a mirror of the published simulation data (SetFrameStorage,
     * Docs/threading-model.md section 3). Each frame, call RunFixedAt (zero or more simulation
     * ticks, each followed by SetTickOutput's function), then RunFrameAt (the variable rate groups).
     *
     * The fixed part (RunFixedAt, RunTick, the timeline, the ledger) and the frame part
     * (RunFrameAt) share no member that either writes, so they can run on two threads
     * when they use two storages.
     *
     * The runner owns the tick timeline (which tick covers which time on the platform
     * clock) and the tick ledger (a record of each tick). A tick is due when the clock
     * reaches the end of its window; see TickTimeline for hitches and pauses.
     *
     * One system runs at a time, in schedule order. Each system has its own command
     * buffer, merged in schedule order at the end of the tick, so a parallel
     * executor can replace this one later with the same results.
     */
    class SystemRunner
    {
    public:
        /** The systems must outlive the runner. The schedule must come from systems.BuildSchedule() and have no errors.
         */
        SystemRunner(const SystemRegistry& systems,
                     Schedule schedule,
                     EntityStorage& storage,
                     SystemRunnerConfig config = {});
        ~SystemRunner();

        SystemRunner(const SystemRunner&) = delete;
        SystemRunner& operator=(const SystemRunner&) = delete;

        /**
         * Makes the singletons that the runner writes (SimTime and InputState in the
         * simulation storage, RealFrameTime and PendingInput in the frame storage), then
         * each singleton that a system reads or writes and each queue that a system
         * consumes, with its default values, if no one made it before (see
         * ComponentInfo::External): a Simulation type in the simulation storage (a mirror
         * gets it from the publish), a Presentation type in the frame storage. Then calls
         * OnCreate in schedule order.
         */
        void Create();

        /** Calls OnDestroy in reverse schedule order, then removes the singletons that Create made. */
        void Destroy();

        /**
         * Run the frame side against another storage (a mirror), or against the simulation
         * storage again. After Create, the variable rate systems are destroyed in the old
         * storage (OnDestroy, their singletons) and made in the new one. Call it with no
         * fixed tick running.
         */
        void SetFrameStorage(EntityStorage& frame);

        /** The storage the variable rate groups run against. */
        EntityStorage& GetFrameStorage() const { return *m_FrameStorage; }

        /** Calls OnPlayBegin on each system that runs in the role, then applies their commands. */
        void PlayBegin(WorldRole role);

        /**
         * As PlayBegin, for the systems of one rate only. A world that shows a copy calls it
         * for Fixed, makes the play state and its first packet, then calls it for Variable,
         * so the frame systems begin on the state that play starts with.
         */
        void PlayBegin(WorldRole role, UpdateRate rate);

        /** Calls OnPlayEnd on each system that runs in the role. */
        void PlayEnd(WorldRole role);

        /**
         * Set the function that fills the input of each fixed tick. The runner calls it once
         * before each tick, into the InputState singleton, before any fixed system runs. It
         * is the only way input reaches a tick. With no source, every tick has no input.
         */
        void SetTickInputSource(TickInputSource t_Source) { m_TickInputSource = std::move(t_Source); }

        /** Set the function that hashes the state after each tick (TickRecord::StateHash). With none, it is 0. */
        void SetStateHasher(StateHasher t_Hasher) { m_StateHasher = std::move(t_Hasher); }

        /**
         * Set the function that reads the input after a sequence. Before the variable rate
         * systems of each frame, the runner fills the PendingInput singleton with it: the
         * input after the cut of the tick that the frame shows. With no source, or before the
         * first tick, PendingInput is empty.
         */
        void SetPendingInputSource(PendingInputSource t_Source) { m_PendingInputSource = std::move(t_Source); }

        /** Set the function that takes each tick's output, on the thread that runs the tick. With none, nothing. */
        void SetTickOutput(TickOutput t_Output) { m_TickOutput = std::move(t_Output); }

        /**
         * Moves the runner's own clock by realDelta, then RunFixedAt that time. The clock
         * starts at 0, so this behaves like an accumulator. Tests use it.
         */
        uint32 RunFixed(std::chrono::nanoseconds realDelta, WorldRole role);

        /**
         * Runs each tick whose window ended at or before nowNs (the platform clock), at most
         * SystemRunnerConfig::MaxTicksPerWake, then drops the time that is left if more are due. The first call
         * starts the timeline at nowNs, unless StartTimeline did. With OneTickPerFrame, runs
         * one tick on request instead. Never runs more than tickLimit ticks (a stop tick).
         * Returns the number of ticks run.
         */
        uint32 RunFixedAt(int64 nowNs, WorldRole role, uint32 tickLimit = UINT32_MAX);

        /** Runs exactly one tick on request (not timed): every fixed group, then the deferred commands. */
        void RunTick(WorldRole role);

        /**
         * Moves the frame clock by realDelta, then RunFrameAt that time. With RunFixed and
         * RunFrame given the same deltas (tests, one thread), the tick and frame clocks agree.
         */
        void RunFrame(std::chrono::nanoseconds realDelta, WorldRole role);

        /** As RunFrameAt with a stamp, with the latest tick of the ledger as the one the frame storage holds. */
        void RunFrameAt(int64 nowNs, std::chrono::nanoseconds realDelta, WorldRole role);

        /**
         * Writes RealFrameTime for a frame at nowNs (the platform clock), then runs the
         * variable rate groups. One thread only. latest is the tick the frame storage holds
         * (a packet header, not the ledger): RealFrameTime::Latest, and the cut that
         * PendingInput follows. blend is what the frame draws (FrameBlend::Advance): the
         * stamps and the factor go to RealFrameTime, the values to ctx.BlendFrom<T> and
         * ctx.BlendTo<T>. With none (Edit, or no presentation link) both ends are latest and
         * the factor is 1. It reads no member that the fixed part writes. Sets the frame clock.
         */
        void RunFrameAt(int64 nowNs,
                        std::chrono::nanoseconds realDelta,
                        WorldRole role,
                        const TickStamp& latest,
                        const BlendFrame* blend = nullptr);

        /** The stamp of the latest tick in the ledger. Tick 0 before the first tick. */
        TickStamp LatestTickStamp() const;

        /** Start the timeline so the first tick's window starts at t_StartNs (before the first RunFixedAt). */
        void StartTimeline(int64 t_StartNs) { m_Timeline.Start(t_StartNs); }

        /** Pause or resume the ticks at the platform clock time nowNs (TickTimeline::Pause). */
        void SetPaused(bool paused, int64 nowNs);

        /**
         * The tick thread, between two ticks: the simulation rate is t_Hz from the next tick
         * on. That tick's window starts where the last one ended and is one tick at t_Hz long
         * (TickTimeline::SetStep), and SimTime::FixedDeltaNs, SimTime::ElapsedNs, the physics
         * step and MaxTicksPerWake follow from it. No tick number is skipped. Returns false
         * when the rate is t_Hz already.
         */
        bool SetSimHz(uint32 t_Hz);

        /** The tick thread: the rate and the limits the ticks run with now. */
        const SystemRunnerConfig& GetConfig() const { return m_Config; }

        bool IsPaused() const { return m_Timeline.IsPaused(); }

        const TickTimeline& GetTimeline() const { return m_Timeline; }

        /** One record per tick that ran (the last TickLedger::Capacity). */
        const TickLedger& GetLedger() const { return m_Ledger; }

        const Schedule& GetSchedule() const { return m_Schedule; }

        /** The number of ticks run since Create. */
        uint64 TickCount() const { return m_Tick; }

    private:
        /** Runs one tick. Returns false when the tick output ends the wake. */
        bool RunOneTick(TickTrigger trigger, WorldRole role);
        void RunSegments(UpdateRate rate, WorldRole role);
        void FlushCommands();
        bool RunsIn(uint32 system, WorldRole role) const;
        bool IsFixed(uint32 system) const;

        /** The storage that a system's data lives in: simulation for a fixed system, frame otherwise. */
        EntityStorage& StorageOf(uint32 system) const;

        /** Prepare (SystemDesc::Prepare) each variable rate system in the frame storage. */
        void PrepareFrameSide();

        /** Remove the variable rate systems' singletons, RealFrameTime and PendingInput from the frame storage. */
        void RemoveFrameSingletons();

        /** The context of OnPlayBegin and OnPlayEnd: as in a tick for a fixed system, as in a frame otherwise. */
        SystemContextBase PlayContext(uint32 system);

        const SystemRegistry& m_Systems;
        Schedule m_Schedule;
        std::vector<uint32> m_RunOrder;
        /** The simulation storage: the fixed groups and their commands. */
        EntityStorage& m_Storage;
        /** The variable rate groups' storage. m_Storage unless SetFrameStorage gave a mirror. */
        EntityStorage* m_FrameStorage = nullptr;
        /** The last frame's values at blend factor 0 and 1 (null: the latest), for the frame contexts. */
        const HeldValues* m_BlendFrom = nullptr;
        const HeldValues* m_BlendTo = nullptr;
        SystemRunnerConfig m_Config;
        /** The last tick before the rate changed to the current one (0: it never changed), and its SimTime::ElapsedNs.
         */
        uint64 m_RateBaseTick = 0;
        int64 m_RateBaseNs = 0;

        TickInputSource m_TickInputSource;

        StateHasher m_StateHasher;

        TickTimeline m_Timeline;

        TickLedger m_Ledger;

        /**
         * The fixed part's clock (the tick thread): the latest time given to RunFixedAt or
         * SetPaused, or the sum of the RunFixed deltas. The timeline starts at it.
         */
        int64 m_TickClockNs = 0;

        /** The frame part's clock (the main thread): the latest RunFrameAt time, or the sum of the RunFrame deltas. */
        int64 m_FrameClockNs = 0;

        /** @see SetTickOutput */
        TickOutput m_TickOutput;

        /** Tick lengths dropped by the last RunFixedAt, for the next tick's record. */
        uint64 m_DroppedSteps = 0;

        PendingInputSource m_PendingInputSource;

        /** The number of RunFixed calls, for FramesPerTick. */
        uint64 m_FixedCalls = 0;

        /** One buffer per system, by index in the schedule. */
        std::vector<DeferredCommands> m_Commands;

        /** One deferred add or remove at the flush, with the order it was made and the order its entity first appeared
         */
        struct FlushEntry
        {
            Entity Target;
            uint32 Sequence = 0;
            uint32 FirstSeen = 0;
            Detail::PendingChange Change;
        };

        /** Scratch for FlushCommands. It keeps its memory, so a steady tick does not allocate. */
        std::vector<FlushEntry> m_FlushEntries;
        std::vector<Detail::PendingChange> m_FlushBatch;
        std::vector<Entity> m_DestroyScratch;

        /**
         * One function for each singleton that Create made for a system, of the fixed systems
         * (in the simulation storage) and of the variable rate systems (in the frame storage).
         * Destroy calls them in reverse.
         */
        std::vector<void (*)(EntityStorage&)> m_OwnedSingletons;
        std::vector<void (*)(EntityStorage&)> m_OwnedFrameSingletons;

        /** Frame side only: the sum of the frame deltas. */
        std::chrono::nanoseconds m_RealElapsed{0};
        uint64 m_Tick = 0;
        bool m_Created = false;
    };
} // namespace Fling
