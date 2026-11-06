#include "Ecs/SimTime.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"

#include <cassert>
#include <ranges>

namespace Fling
{
    namespace
    {
        constexpr Phase FixedPhases[] = {Phase::Input, Phase::Simulation, Phase::ResolveDeferred};
        constexpr Phase FramePhases[] = {Phase::PostSimulation, Phase::Extract, Phase::Editor};

        double Seconds(const std::chrono::nanoseconds time)
        {
            return std::chrono::duration<double>(time).count();
        }
    } // namespace

    SystemRunner::SystemRunner(const SystemRegistry& systems,
                               Schedule schedule,
                               entt::registry& storage,
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

        m_Storage.set<SimTime>(SimTime{0, static_cast<float>(Seconds(m_Config.FixedStep)), 0.0});
        m_Storage.set<RealFrameTime>();

        for (const uint32 i : m_RunOrder)
        {
            if (const auto prepare = m_Systems.At(i).Describe().PrepareStorage)
            {
                prepare(setup);
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

        m_Storage.unset<RealFrameTime>();
        m_Storage.unset<SimTime>();
        m_Created = false;
    }

    void SystemRunner::PlayBegin(const WorldRole role)
    {
        for (const uint32 i : m_RunOrder)
        {
            if (RunsIn(i, role))
            {
                SystemContextBase ctx(m_Storage, &m_Commands[i]);
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
                SystemContextBase ctx(m_Storage, &m_Commands[i]);
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

        SimTime& time = m_Storage.ctx<SimTime>();
        time.Tick = m_Tick;
        time.Elapsed = static_cast<double>(m_Tick) * Seconds(m_Config.FixedStep);

        for (const Phase phase : FixedPhases)
        {
            RunPhase(phase, role);
        }
        FlushCommands();
    }

    void SystemRunner::RunFrame(const std::chrono::nanoseconds realDelta, const WorldRole role)
    {
        assert(m_Created);
        m_RealElapsed += realDelta;

        RealFrameTime& frame = m_Storage.ctx<RealFrameTime>();
        frame.DeltaSeconds = static_cast<float>(Seconds(realDelta));
        frame.Elapsed = Seconds(m_RealElapsed);
        frame.TickAlpha =
            m_Config.OneTickPerFrame ? 1.0f : static_cast<float>(Seconds(m_Accumulator) / Seconds(m_Config.FixedStep));

        for (const Phase phase : FramePhases)
        {
            RunPhase(phase, role);
        }
    }

    void SystemRunner::RunPhase(const Phase phase, const WorldRole role)
    {
        const bool fixed = IsFixedPhase(phase);
        for (const ScheduleStage& stage : m_Schedule.GetPhase(phase).Stages)
        {
            for (const uint32 i : stage.Systems)
            {
                if (!RunsIn(i, role))
                {
                    continue;
                }

                // A frame phase gets no command buffer: it cannot change the simulation.
                SystemContextBase ctx(m_Storage, fixed ? &m_Commands[i] : nullptr);
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
    }

    void SystemRunner::FlushCommands()
    {
        for (const uint32 i : m_RunOrder)
        {
            CommandBuffer& commands = m_Commands[i];
            for (const Entity entity : commands.Destroys())
            {
                const entt::entity e = Detail::ToEntt(entity);
                if (e != entt::null && m_Storage.valid(e))
                {
                    m_Storage.destroy(e);
                }
            }
            commands.Clear();
        }
    }

    bool SystemRunner::RunsIn(const uint32 system, const WorldRole role) const
    {
        return Overlaps(m_Schedule.Systems[system]->Info.RunIn, role);
    }
} // namespace Fling
