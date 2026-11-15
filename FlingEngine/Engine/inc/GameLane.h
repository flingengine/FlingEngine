#pragma once

#include "FlingTypes.h"
#include "Threading/Event.h"
#include "Threading/Semaphore.h"
#include "Threading/ThreadJitter.h"
#include "Threading/TickLimiter.h"
#include "TickDriver.h"

#include <atomic>
#include <memory>

namespace Fling
{
    class Thread;

    /** How the game thread decides when to wake. */
    enum class GameLanePacing : uint8
    {
        /** Sleep until the next tick is due on the host's clock (a windowed game). */
        RealTime,

        /**
         * Wake again at once; after a wake that ran no tick, yield first. For a clock that
         * another thread moves (the virtual clock of a headless run): the lane runs each
         * tick as soon as the clock makes it due.
         */
        AsFastAsPossible,

        /** Each wake waits for a permit from Step (tests). */
        Stepped,
    };

    /** How a world's game thread runs: data only. Set it with World::SetGameLaneDesc before play starts. */
    struct GameLaneDesc
    {
        GameLanePacing Pacing = GameLanePacing::RealTime;

        /** --thread-jitter on the game thread: a sleep before each wake and after each wake that ran a tick. 0: off. */
        uint32 JitterSeed = 0;
        uint32 JitterMaxMicroseconds = 0;
    };

    /**
     * The game thread (OS name "Game Thread"), the TickDriver of Threaded play:
     * it runs only the wakes of one TickHost (World::RunWake). It never waits for the main
     * thread, except inside a tick that waits for input (TickInputProvider) until
     * ShouldStopWaiting. While it runs, the world's simulation storage belongs to Lane::Game.
     *
     * Start, Step and the destructor are for the main thread.
     */
    class GameLane final : public TickDriver
    {
    public:
        GameLane(TickHost& t_Host, const GameLaneDesc& t_Desc);

        /** Stops the thread after its current wake and joins it. */
        ~GameLane() override;

        GameLane(const GameLane&) = delete;
        GameLane& operator=(const GameLane&) = delete;

        /** Start the thread. False if the OS cannot start it. */
        bool Start();

        /** Stepped pacing: let t_Wakes wakes run, and wait until they are done or the lane has finished. */
        void Step(uint32 t_Wakes = 1);

        void OnFrame() override {}

        void OnClockCommand() override { m_WakeUp.Signal(); }

        bool ShouldStopWaiting() const override { return IsStopRequested(); }

        /** True after the wake that ran the last tick, or after the stop. */
        bool IsFinished() const { return m_Finished.load(std::memory_order_acquire); }

        /** Wakes that ran, for statistics and tests. */
        uint64 WakeCount() const { return m_Wakes.load(std::memory_order_acquire); }

    private:
        /** The loop of the game thread. */
        void Run();

        /** Stepped pacing: wait for a permit. False when the lane must stop instead. */
        bool WaitForPermit();

        /** One wake of the host, with the jitter around it. Returns the ticks that ran. */
        uint32 Wake();

        /** Between wakes: sleep while paused, until the next tick (RealTime), or yield after an empty wake. */
        void WaitForNextWake(uint32 t_TicksRun);

        bool IsStopRequested() const { return m_StopRequested.load(std::memory_order_acquire); }

        TickHost& m_Host;
        const GameLaneDesc m_Desc;
        std::shared_ptr<Thread> m_Thread;

        std::atomic<bool> m_StopRequested{false};
        std::atomic<bool> m_Finished{false};
        std::atomic<uint64> m_Wakes{0};

        /** Stepped pacing: one permit per wake, and one release per finished wake. */
        Semaphore m_Permits;
        Semaphore m_WakesDone;

        /** A paused lane sleeps on it. A clock command and the stop signal it. */
        Event m_WakeUp{EventReset::Auto};

        /** RealTime pacing: the interruptible high-resolution wait until the next tick. */
        TickLimiter m_Waiter;

        /** Game thread only. */
        ThreadJitter m_Jitter;
    };
} // namespace Fling
