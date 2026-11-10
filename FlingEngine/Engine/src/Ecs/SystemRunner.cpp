#include "Ecs/SimTime.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"
#include "HierarchyUtil.h"
#include "Misc/SimulationCheck.h"

#include <algorithm>
#include <cassert>
#include <optional>
#include <ranges>

namespace Fling
{
    namespace
    {
        double Seconds(const std::chrono::nanoseconds time)
        {
            return std::chrono::duration<double>(time).count();
        }
    } // namespace

    SystemRunner::SystemRunner(const SystemRegistry& systems,
                               Schedule schedule,
                               EntityStorage& storage,
                               SystemRunnerConfig config)
        : m_Systems(systems), m_Schedule(std::move(schedule)), m_Storage(storage), m_Config(config)
    {
        assert(m_Schedule.Systems.size() == m_Systems.Size() && "The schedule must come from these systems");
        assert(m_Config.FixedStep.count() > 0);
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
        SystemSetupBase setup(m_Storage);

        m_Storage.SetSingleton<SimTime>(SimTime{0, static_cast<float>(Seconds(m_Config.FixedStep)), 0.0});
        m_Storage.SetSingleton<RealFrameTime>();

        // Every owned singleton exists before the first OnCreate, so an OnCreate can read another system's
        for (const uint32 i : m_RunOrder)
        {
            if (const auto prepare = m_Systems.At(i).Describe().Prepare)
            {
                prepare(m_Storage, m_OwnedSingletons);
            }
        }
        for (const uint32 i : m_RunOrder)
        {
            m_Systems.At(i).RunCreate(setup);
        }
        m_Created = true;
    }

    void SystemRunner::Destroy()
    {
        assert(m_Created);
        SystemSetupBase setup(m_Storage);
        for (const uint32 i : std::views::reverse(m_RunOrder))
        {
            m_Systems.At(i).RunDestroy(setup);
        }
        for (const auto remove : std::views::reverse(m_OwnedSingletons))
        {
            remove(m_Storage);
        }
        m_OwnedSingletons.clear();

        m_Storage.RemoveSingleton<RealFrameTime>();
        m_Storage.RemoveSingleton<SimTime>();
        m_Created = false;
    }

    void SystemRunner::PlayBegin(const WorldRole role)
    {
        // OnPlayBegin counts as setup for tick 0, so it follows the simulation rules.
        const SimulationCheck::Scope simulation;
        for (const uint32 i : m_RunOrder)
        {
            if (RunsIn(i, role))
            {
                SystemContextBase ctx(m_Storage, &m_Commands[i], &m_TickInput);
                m_Systems.At(i).RunPlayBegin(ctx);
            }
        }
        FlushCommands();
    }

    void SystemRunner::PlayEnd(const WorldRole role)
    {
        for (const uint32 i : m_RunOrder)
        {
            if (RunsIn(i, role))
            {
                SystemContextBase ctx(m_Storage, &m_Commands[i], &m_TickInput);
                m_Systems.At(i).RunPlayEnd(ctx);
            }
        }
        FlushCommands();
    }

    uint32 SystemRunner::RunFixed(const std::chrono::nanoseconds realDelta, const WorldRole role)
    {
        if (m_Config.OneTickPerFrame)
        {
            RunTick(role);
            return 1;
        }

        m_Accumulator += realDelta;
        uint32 ticks = 0;
        while (m_Accumulator >= m_Config.FixedStep && ticks < m_Config.MaxTicksPerFrame)
        {
            RunTick(role);
            m_Accumulator -= m_Config.FixedStep;
            ++ticks;
        }

        // After a stall, drop the time that is left instead of running more ticks next frame.
        if (m_Accumulator >= m_Config.FixedStep)
        {
            m_Accumulator %= m_Config.FixedStep;
        }
        return ticks;
    }

    void SystemRunner::RunTick(const WorldRole role)
    {
        assert(m_Created);
        ++m_Tick;

        // Each tick takes the input that no earlier tick took
        if (m_TickInputSource)
        {
            m_TickInputSource(m_TickInput);
        }
        else
        {
            m_TickInput.Clear();
        }

        SimTime& time = m_Storage.Singleton<SimTime>();
        time.Tick = m_Tick;
        time.Elapsed = static_cast<double>(m_Tick) * Seconds(m_Config.FixedStep);

        RunSegments(UpdateRate::Fixed, role);
        FlushCommands();
    }

    void SystemRunner::RunFrame(const std::chrono::nanoseconds realDelta, const WorldRole role)
    {
        assert(m_Created);
        m_RealElapsed += realDelta;

        RealFrameTime& frame = m_Storage.Singleton<RealFrameTime>();
        frame.DeltaSeconds = static_cast<float>(Seconds(realDelta));
        frame.Elapsed = Seconds(m_RealElapsed);
        frame.TickAlpha =
            m_Config.OneTickPerFrame ? 1.0f : static_cast<float>(Seconds(m_Accumulator) / Seconds(m_Config.FixedStep));

        RunSegments(UpdateRate::Frame, role);
    }

    void SystemRunner::RunSegments(const UpdateRate rate, const WorldRole role)
    {
        const bool fixed = rate == UpdateRate::Fixed;
        std::optional<SimulationCheck::Scope> simulation;
        if (fixed)
        {
            simulation.emplace();
        }
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

                    // A frame phase gets no command buffer: it cannot change the simulation.
                    SystemContextBase ctx(m_Storage, fixed ? &m_Commands[i] : nullptr, fixed ? &m_TickInput : nullptr);
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
            if (fixed && segment.FlushPhase != nullptr)
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
} // namespace Fling
