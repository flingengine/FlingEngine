#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/Publish/FrameBlend.h"
#include "Ecs/RunnerWritten.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"
#include "HierarchyUtil.h"
#include "MathUtils.h"
#include "Misc/SimulationCheck.h"
#include "PlatformTime.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <ranges>
#include <utility>

namespace Fling
{
    namespace
    {
        double Seconds(const std::chrono::nanoseconds time)
        {
            return std::chrono::duration<double>(time).count();
        }

        /** Makes each singleton of a RunnerWritten list, with its default values. */
        template <typename... Ts>
        void AddSingletons(EntityStorage& storage, TypeList<Ts...>)
        {
            (storage.SetSingleton<Ts>(), ...);
        }

        template <typename... Ts>
        void RemoveSingletons(EntityStorage& storage, TypeList<Ts...>)
        {
            (storage.RemoveSingleton<Ts>(), ...);
        }
    } // namespace

    SystemRunner::SystemRunner(const SystemRegistry& systems,
                               Schedule schedule,
                               EntityStorage& storage,
                               SystemRunnerConfig config)
        : m_Systems(systems),
          m_Schedule(std::move(schedule)),
          m_Storage(storage),
          m_FrameStorage(&storage),
          m_Config(config),
          m_Timeline(config.FixedStep().count())
    {
        assert(m_Schedule.Systems.size() == m_Systems.Size() && "The schedule must come from these systems");
        assert(SimRate::IsValidHz(m_Config.SimHz));
        m_RunOrder = m_Schedule.RunOrder();
        m_Commands.resize(m_Systems.Size());
    }

    SystemRunner::~SystemRunner()
    {
        if (m_Created)
        {
            Destroy();
        }
    }

    void SystemRunner::Create()
    {
        assert(!m_Created);
        AddSingletons(m_Storage, RunnerWritten<UpdateRate::Fixed>{});
        m_Storage.Singleton<SimTime>() = SimTime{.Tick = 0, .FixedDeltaNs = m_Config.FixedStep().count()};
        AddSingletons(*m_FrameStorage, RunnerWritten<UpdateRate::Variable>{});

        // Every singleton a system names exists before the first OnCreate, so an OnCreate can read another system's
        for (const uint32 i : m_RunOrder)
        {
            const auto prepare = m_Systems.At(i).Describe().Prepare;
            if (prepare != nullptr)
            {
                prepare(m_Storage, ComponentDomain::Simulation, m_OwnedSingletons);
            }
        }
        PrepareFrameSide();
        for (const uint32 i : m_RunOrder)
        {
            SystemSetupBase setup(StorageOf(i));
            m_Systems.At(i).RunCreate(setup);
        }
        m_Created = true;
    }

    void SystemRunner::Destroy()
    {
        assert(m_Created);
        for (const uint32 i : std::views::reverse(m_RunOrder))
        {
            SystemSetupBase setup(StorageOf(i));
            m_Systems.At(i).RunDestroy(setup);
        }
        RemoveFrameSingletons();
        for (const auto remove : std::views::reverse(m_OwnedSingletons))
        {
            remove(m_Storage);
        }
        m_OwnedSingletons.clear();
        RemoveSingletons(m_Storage, RunnerWritten<UpdateRate::Fixed>{});
        m_Created = false;
    }

    void SystemRunner::SetFrameStorage(EntityStorage& frame)
    {
        if (&frame == m_FrameStorage)
        {
            return;
        }
        if (!m_Created)
        {
            m_FrameStorage = &frame;
            return;
        }

        // The variable rate systems leave the old storage and are made again in the new one
        for (const uint32 i : std::views::reverse(m_RunOrder))
        {
            if (!IsFixed(i))
            {
                SystemSetupBase setup(*m_FrameStorage);
                m_Systems.At(i).RunDestroy(setup);
            }
        }
        RemoveFrameSingletons();
        m_FrameStorage = &frame;
        AddSingletons(*m_FrameStorage, RunnerWritten<UpdateRate::Variable>{});
        PrepareFrameSide();
        for (const uint32 i : m_RunOrder)
        {
            if (!IsFixed(i))
            {
                SystemSetupBase setup(*m_FrameStorage);
                m_Systems.At(i).RunCreate(setup);
            }
        }
    }

    void SystemRunner::PrepareFrameSide()
    {
        for (const uint32 i : m_RunOrder)
        {
            const auto prepare = m_Systems.At(i).Describe().Prepare;
            if (prepare != nullptr && !IsFixed(i))
            {
                prepare(*m_FrameStorage, ComponentDomain::Presentation, m_OwnedFrameSingletons);
            }
        }
    }

    void SystemRunner::RemoveFrameSingletons()
    {
        for (const auto remove : std::views::reverse(m_OwnedFrameSingletons))
        {
            remove(*m_FrameStorage);
        }
        m_OwnedFrameSingletons.clear();
        RemoveSingletons(*m_FrameStorage, RunnerWritten<UpdateRate::Variable>{});
    }

    void SystemRunner::PlayBegin(const WorldRole role)
    {
        PlayBegin(role, UpdateRate::Fixed);
        PlayBegin(role, UpdateRate::Variable);
    }

    void SystemRunner::PlayBegin(const WorldRole role, const UpdateRate rate)
    {
        const bool fixed = rate == UpdateRate::Fixed;
        // A fixed system's OnPlayBegin counts as setup for tick 0, so it follows the simulation rules.
        std::optional<SimulationCheck::Scope> simulation;
        if (fixed)
        {
            simulation.emplace();
        }
        for (const uint32 i : m_RunOrder)
        {
            if (IsFixed(i) == fixed && RunsIn(i, role))
            {
                SystemContextBase ctx = PlayContext(i);
                m_Systems.At(i).RunPlayBegin(ctx);
            }
        }
        if (fixed)
        {
            FlushCommands();
        }
    }

    void SystemRunner::PlayEnd(const WorldRole role)
    {
        for (const uint32 i : m_RunOrder)
        {
            if (RunsIn(i, role))
            {
                SystemContextBase ctx = PlayContext(i);
                m_Systems.At(i).RunPlayEnd(ctx);
            }
        }
        FlushCommands();
    }

    SystemContextBase SystemRunner::PlayContext(const uint32 system)
    {
        // A variable rate system gets its own storage and no commands, as in a frame
        if (IsFixed(system))
        {
            return SystemContextBase(m_Storage, &m_Commands[system]);
        }
        return SystemContextBase(*m_FrameStorage, nullptr, m_BlendFrom, m_BlendTo);
    }

    uint32 SystemRunner::RunFixed(const std::chrono::nanoseconds realDelta, const WorldRole role)
    {
        // The runner's own clock starts at 0 with the timeline, like an accumulator
        if (!m_Timeline.IsStarted())
        {
            m_Timeline.Start(m_TickClockNs);
        }
        return RunFixedAt(m_TickClockNs + realDelta.count(), role);
    }

    uint32 SystemRunner::RunFixedAt(const int64 nowNs, const WorldRole role, const uint32 tickLimit)
    {
        m_TickClockNs = nowNs;
        if (m_Config.OneTickPerFrame)
        {
            if (m_Timeline.IsPaused())
            {
                return 0;
            }
            const uint64 call = m_FixedCalls++;
            if (tickLimit == 0 || (m_Config.FramesPerTick > 1 && call % m_Config.FramesPerTick != 0))
            {
                return 0;
            }
            RunOneTick(TickTrigger::Request, role);
            return 1;
        }

        if (!m_Timeline.IsStarted())
        {
            m_Timeline.Start(nowNs);
        }
        const uint32 maxTicks = m_Config.MaxTicksPerWake();
        const uint32 limit = Math::Min(maxTicks, tickLimit);
        uint32 ticks = 0;
        bool goOn = true;
        while (goOn && ticks < limit && m_Timeline.IsDue(nowNs))
        {
            goOn = RunOneTick(TickTrigger::Clock, role);
            ++ticks;
        }

        // After a stall, drop the time that is left instead of running more ticks next
        // frame. The next tick keeps its number; its window is the one that holds now. A
        // wake that a tick limit or the tick output ended keeps its due ticks for later.
        if (ticks >= maxTicks)
        {
            m_DroppedSteps += m_Timeline.DropBacklog(nowNs);
        }
        return ticks;
    }

    void SystemRunner::RunTick(const WorldRole role)
    {
        RunOneTick(TickTrigger::Request, role);
    }

    void SystemRunner::SetPaused(const bool paused, const int64 nowNs)
    {
        m_TickClockNs = nowNs;
        if (paused)
        {
            m_Timeline.Pause(nowNs);
        }
        else
        {
            m_Timeline.Resume(nowNs);
        }
    }

    bool SystemRunner::SetSimHz(const uint32 t_Hz)
    {
        assert(SimRate::IsValidHz(t_Hz));
        if (t_Hz == m_Config.SimHz)
        {
            return false;
        }
        m_RateBaseNs += static_cast<int64>(m_Tick - m_RateBaseTick) * m_Config.FixedStep().count();
        m_RateBaseTick = m_Tick;
        m_Config.SimHz = t_Hz;
        m_Timeline.SetStep(m_Config.FixedStep().count());
        return true;
    }

    bool SystemRunner::RunOneTick(const TickTrigger trigger, const WorldRole role)
    {
        assert(m_Created);
        if (!m_Timeline.IsStarted())
        {
            m_Timeline.Start(m_TickClockNs);
        }
        assert(m_Timeline.NextTick() == m_Tick + 1 && "The timeline and the runner count the same ticks");
        TickRecord record;
        record.Stamp.Window = m_Timeline.Advance(trigger);
        record.Stamp.Tick = ++m_Tick;
        record.Stamp.FixedDeltaNs = m_Config.FixedStep().count();
        record.Stamp.ElapsedNs = m_RateBaseNs + static_cast<int64>(m_Tick - m_RateBaseTick) * record.Stamp.FixedDeltaNs;
        record.RanAtNs = PlatformTime::NowNanoseconds();

        // Each tick takes the input of its window that no earlier tick took. The runner writes it
        // into InputState before any system runs, and no system can write InputState, so a system
        // gets the input only by declaring Read<InputState>.
        TickInput& input = m_Storage.Singleton<InputState>().Tick;
        if (m_TickInputSource)
        {
            m_TickInputSource(input, record.Stamp.Window);
        }
        else
        {
            input.Clear();
        }
        record.Stamp.Cut = input.Slice.Cut;
        record.MouseSamples = static_cast<uint32>(input.Slice.MouseSamples.size());
        record.KeyEvents = static_cast<uint32>(input.Slice.KeyEvents.size());
        record.InputOverran = input.Slice.Overran;

        // The stamp holds all of SimTime, so a tick can be run again from its ledger row
        m_Storage.Singleton<SimTime>() = SimTimeOf(record.Stamp);

        {
            // The fixed systems and the changes they deferred, to the last flush, are simulation code
            const SimulationCheck::Scope simulation;
            RunSegments(UpdateRate::Fixed, role);
            FlushCommands();
        }

        record.Stamp.StateHash = m_StateHasher ? m_StateHasher(m_Storage) : 0;
        record.DroppedSteps = std::exchange(m_DroppedSteps, 0);
        m_Ledger.Push(record.Stamp.Tick, record);
        return m_TickOutput ? m_TickOutput(m_Tick) : true;
    }

    void SystemRunner::RunFrame(const std::chrono::nanoseconds realDelta, const WorldRole role)
    {
        RunFrameAt(m_FrameClockNs + realDelta.count(), realDelta, role);
    }

    void SystemRunner::RunFrameAt(const int64 nowNs, const std::chrono::nanoseconds realDelta, const WorldRole role)
    {
        RunFrameAt(nowNs, realDelta, role, LatestTickStamp());
    }

    TickStamp SystemRunner::LatestTickStamp() const
    {
        const TickRecord* latest = m_Ledger.Latest();
        return latest != nullptr ? latest->Stamp : TickStamp{};
    }

    void SystemRunner::RunFrameAt(const int64 nowNs,
                                  const std::chrono::nanoseconds realDelta,
                                  const WorldRole role,
                                  const TickStamp& shown,
                                  const BlendFrame* blend)
    {
        assert(m_Created);
        m_FrameClockNs = nowNs;
        m_RealElapsed += realDelta;

        // Copies, not the timeline or the ledger: on a thread split this is what the frame side is given
        RealFrameTime& frame = m_FrameStorage->Singleton<RealFrameTime>();
        frame.DeltaSeconds = static_cast<float>(Seconds(realDelta));
        frame.Elapsed = Seconds(m_RealElapsed);
        frame.FrameNs = nowNs;
        frame.Latest = shown;

        // What the frame draws comes from the frame side's blend (FrameBlend); without one, the latest
        frame.BlendFrom = blend != nullptr ? blend->FromStamp : shown;
        frame.BlendTo = blend != nullptr ? blend->ToStamp : shown;
        frame.BlendAlpha = blend != nullptr ? blend->Alpha : 1.0f;
        m_BlendFrom = blend != nullptr ? blend->From : nullptr;
        m_BlendTo = blend != nullptr ? blend->To : nullptr;

        // The input after the cut of the shown tick, made from scratch each frame. It consumes
        // nothing, so the next tick still takes every message. Before the first tick there is
        // no cut, and nothing is pending.
        PendingInput& pending = m_FrameStorage->Singleton<PendingInput>();
        if (shown.Tick > 0 && m_PendingInputSource)
        {
            m_PendingInputSource(shown.Cut.LastSequence, pending);
        }
        else
        {
            pending.Clear();
            pending.Cut = InputCut{shown.Cut.LastSequence, shown.Cut.LastSequence};
        }

        RunSegments(UpdateRate::Variable, role);
    }

    void SystemRunner::RunSegments(const UpdateRate rate, const WorldRole role)
    {
        const bool fixed = rate == UpdateRate::Fixed;
        for (const ScheduleSegment& segment : m_Schedule.GetSegments(rate))
        {
            for (const ScheduleStage& stage : segment.Stages)
            {
                for (const uint32 i : stage.Systems)
                {
                    if (!RunsIn(i, role))
                    {
                        continue;
                    }

                    // A variable rate group gets no command buffer: it cannot change the simulation.
                    SystemContextBase ctx(fixed ? m_Storage : *m_FrameStorage,
                                          fixed ? &m_Commands[i] : nullptr,
                                          fixed ? nullptr : m_BlendFrom,
                                          fixed ? nullptr : m_BlendTo);
                    if (fixed)
                    {
                        m_Systems.At(i).RunFixedUpdate(ctx);
                    }
                    else
                    {
                        m_Systems.At(i).RunUpdate(ctx);
                    }
                }
            }
            if (fixed && segment.FlushGroup != nullptr)
            {
                FlushCommands();
            }
        }
    }

    void SystemRunner::FlushCommands()
    {
        // 1. Every system's adds and removes, grouped by entity, entities in the order
        //    they first appear, so each entity moves archetype once. The scratch lists
        //    keep their memory between ticks, and std::sort does not allocate.
        m_FlushEntries.clear();
        uint32 sequence = 0;
        for (const uint32 i : m_RunOrder)
        {
            for (DeferredCommands::ComponentChange& each : m_Commands[i].ComponentChanges())
            {
                m_FlushEntries.push_back(FlushEntry{each.Target, sequence, sequence, each.Change});
                ++sequence;
                // The batch destroys the value now. Its memory stays in the command buffer's arena until Clear.
                each.Change.Value = nullptr;
            }
        }
        if (!m_FlushEntries.empty())
        {
            const auto byEntity = [](const FlushEntry& a, const FlushEntry& b)
            {
                return a.Target.ToBits() != b.Target.ToBits() ? a.Target.ToBits() < b.Target.ToBits()
                                                              : a.Sequence < b.Sequence;
            };
            std::sort(m_FlushEntries.begin(), m_FlushEntries.end(), byEntity);
            for (size_t i = 1; i < m_FlushEntries.size(); ++i)
            {
                if (m_FlushEntries[i].Target == m_FlushEntries[i - 1].Target)
                {
                    m_FlushEntries[i].FirstSeen = m_FlushEntries[i - 1].FirstSeen;
                }
            }
            std::sort(m_FlushEntries.begin(),
                      m_FlushEntries.end(),
                      [](const FlushEntry& a, const FlushEntry& b)
                      { return a.FirstSeen != b.FirstSeen ? a.FirstSeen < b.FirstSeen : a.Sequence < b.Sequence; });

            size_t start = 0;
            while (start < m_FlushEntries.size())
            {
                size_t end = start + 1;
                while (end < m_FlushEntries.size() && m_FlushEntries[end].Target == m_FlushEntries[start].Target)
                {
                    ++end;
                }
                m_FlushBatch.clear();
                for (size_t i = start; i < end; ++i)
                {
                    m_FlushBatch.push_back(m_FlushEntries[i].Change);
                }
                m_Storage.Native().ApplyBatch(m_FlushEntries[start].Target, m_FlushBatch);
                start = end;
            }
        }

        // 2. The other deferred changes, then 3. the destroys, each in schedule order
        for (const uint32 i : m_RunOrder)
        {
            for (const DeferredCommands::StructuralFn& change : m_Commands[i].StructuralChanges())
            {
                change(m_Storage);
            }
        }
        for (const uint32 i : m_RunOrder)
        {
            for (const Entity entity : m_Commands[i].Destroys())
            {
                HierarchyUtil::DestroySubtree(m_Storage, entity, m_DestroyScratch);
            }
            m_Commands[i].Clear();
        }
    }

    bool SystemRunner::RunsIn(const uint32 system, const WorldRole role) const
    {
        return Overlaps(m_Schedule.Systems[system]->Info.RunIn, role);
    }

    bool SystemRunner::IsFixed(const uint32 system) const
    {
        return m_Schedule.Groups[m_Schedule.SystemGroup[system]].Rate == UpdateRate::Fixed;
    }

    EntityStorage& SystemRunner::StorageOf(const uint32 system) const
    {
        return IsFixed(system) ? m_Storage : *m_FrameStorage;
    }
} // namespace Fling
