#pragma once

#include "Ecs/Phase.h"
#include "Ecs/Schedule.h"
#include "FlingTypes.h"

#include <chrono>
#include <entt/entity/fwd.hpp>
#include <vector>

namespace Fling
{
    class CommandBuffer;
    class SystemRegistry;

    struct SystemRunnerConfig
    {
        /** The length of one simulation tick. The default is 60 Hz. */
        std::chrono::nanoseconds FixedStep{16'666'667};
        /** The most ticks one frame may run. After a long stall, the extra time is dropped, not caught up. */
        uint32 MaxTicksPerFrame = 8;
        /** Run exactly one tick per frame, whatever the real time. Headless and --null-render runs use this. */
        bool OneTickPerFrame = false;
    };

    /**
     * Runs a schedule against one world's storage. Each frame, call RunFixed (zero or
     * more simulation ticks), then any legacy update, then RunFrame (the frame phases).
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
                     entt::registry& storage,
                     SystemRunnerConfig config = {});
        ~SystemRunner();

        SystemRunner(const SystemRunner&) = delete;
        SystemRunner& operator=(const SystemRunner&) = delete;

        /** Makes the storage for every declared component and the clock singletons, then calls OnCreate in schedule
         * order. */
        void Create();

        /** Calls OnDestroy in reverse schedule order, then removes the clock singletons. */
        void Destroy();

        /** Calls OnPlayBegin on each system that runs in the role, then applies their commands. */
        void PlayBegin(WorldRole role);

        /** Calls OnPlayEnd on each system that runs in the role. */
        void PlayEnd(WorldRole role);

        /** Adds the real time to the accumulator and runs each tick that is due. Returns the number of ticks run. */
        uint32 RunFixed(std::chrono::nanoseconds realDelta, WorldRole role);

        /** Runs exactly one tick: every fixed phase in schedule order, then the deferred commands. */
        void RunTick(WorldRole role);

        /** Writes RealFrameTime, then runs every frame phase in schedule order. */
        void RunFrame(std::chrono::nanoseconds realDelta, WorldRole role);

        const Schedule& GetSchedule() const { return m_Schedule; }

        /** The number of ticks run since Create. */
        uint64 TickCount() const { return m_Tick; }

    private:
        void RunSegments(UpdateRate rate, WorldRole role);
        void FlushCommands();
        bool RunsIn(uint32 system, WorldRole role) const;

        const SystemRegistry& m_Systems;
        Schedule m_Schedule;
        std::vector<uint32> m_RunOrder;
        entt::registry& m_Storage;
        SystemRunnerConfig m_Config;

        /** One buffer per system, by index in the schedule. */
        std::vector<CommandBuffer> m_Commands;

        std::chrono::nanoseconds m_Accumulator{0};
        std::chrono::nanoseconds m_RealElapsed{0};
        uint64 m_Tick = 0;
        bool m_Created = false;
    };
} // namespace Fling
