#include "MathUtils.h"
#include "Threading/FrameLimiter.h"
#include "Threading/SleepTimer.h"

#include <catch2/catch_all.hpp>
#include <chrono>

// The frame limiter on a virtual clock: the sleep moves the clock (early, late or exact) and
// each spin read moves it by a little, so no test sleeps on the wall clock.

namespace
{
    using namespace Fling;

    constexpr int64 Us = 1'000;
    constexpr int64 Ms = 1'000'000;

    /** A virtual clock, a sleep that wakes t_Oversleep after the asked time, and a spin of SpinReadNs per read. */
    struct VirtualFrames
    {
        static constexpr int64 SpinReadNs = 1 * Us;

        int64 Now = 1'000 * Ms;
        int64 Oversleep = 0;
        int Sleeps = 0;
        int Reads = 0;
        bool InSpin = false;

        FrameLimiter Make(const int64 t_SpinNs)
        {
            return FrameLimiter(
                [this]()
                {
                    ++Reads;
                    // Only the reads after a sleep are a spin; the first read of a wait is free
                    if (InSpin)
                    {
                        Now += SpinReadNs;
                    }
                    InSpin = true;
                    return Now;
                },
                [this](const int64 t_UntilNs)
                {
                    ++Sleeps;
                    Now = Math::Max<int64>(Now, t_UntilNs + Oversleep);
                },
                t_SpinNs);
        }

        /** One frame of work, then the wait. Returns the time the next frame starts. */
        int64 Frame(FrameLimiter& t_Limiter, const int64 t_WorkNs)
        {
            Now += t_WorkNs;
            InSpin = false;
            t_Limiter.WaitForNextFrame();
            return Now;
        }
    };
} // namespace

TEST_CASE("Frame limiter: frames start on a fixed schedule, never early, and the average is the cap", "[FrameLimiter]")
{
    for (const uint32 Fps : {60u, 144u, 240u})
    {
        CAPTURE(Fps);
        VirtualFrames Clock;
        FrameLimiter Limiter = Clock.Make(1 * Ms);
        Limiter.SetMaxFps(Fps);
        const int64 Period = Limiter.GetPeriodNs();
        REQUIRE(Period == (1'000'000'000 + Fps / 2) / Fps);

        // The first wait starts the schedule
        const int64 Start = Clock.Frame(Limiter, 0);
        constexpr int Frames = 1000;
        int64 Last = Start;
        for (int i = 1; i <= Frames; ++i)
        {
            // Work from 0 to 90% of the frame, and a sleep that wakes up to 0.9 ms late: the spin covers it
            Clock.Oversleep = (i * 37 % 10) * 90 * Us;
            const int64 Begin = Clock.Frame(Limiter, (i * 53 % 10) * Period / 11);
            const int64 Due = Start + i * Period;
            REQUIRE(Begin >= Due);
            REQUIRE(Begin <= Due + VirtualFrames::SpinReadNs);
            Last = Begin;
        }
        // No drift: the average frame is the cap, to the spin read
        const double AverageNs = static_cast<double>(Last - Start) / Frames;
        REQUIRE(AverageNs == Catch::Approx(static_cast<double>(Period)).margin(1.0));
        REQUIRE(Limiter.GetRestarts() == 0);
    }
}

TEST_CASE("Frame limiter: it sleeps for most of the wait and spins only the margin", "[FrameLimiter]")
{
    VirtualFrames Clock;
    FrameLimiter Limiter = Clock.Make(1 * Ms);
    Limiter.SetMaxFps(100);
    Clock.Frame(Limiter, 0);

    Clock.Reads = 0;
    Clock.Frame(Limiter, 2 * Ms);
    REQUIRE(Clock.Sleeps == 1);
    // A 10 ms frame with 2 ms of work: one sleep to 1 ms before the deadline, then about 1 ms of spin reads
    REQUIRE(Clock.Reads <= 1 + (1 * Ms) / VirtualFrames::SpinReadNs + 1);

    // The spin margin is at most half a frame
    Limiter.SetMaxFps(1000);
    REQUIRE(Limiter.GetSpinNs() == Limiter.GetPeriodNs() / 2);
}

TEST_CASE("Frame limiter: a late frame keeps the schedule; a very late one starts it again", "[FrameLimiter]")
{
    VirtualFrames Clock;
    FrameLimiter Limiter = Clock.Make(1 * Ms);
    Limiter.SetMaxFps(100);
    const int64 Period = Limiter.GetPeriodNs();
    const int64 Start = Clock.Frame(Limiter, 0);

    // A sleep that wakes 3 ms past the deadline (more than the spin margin): late, but the
    // next frame is due on the old schedule, so it is shorter
    Clock.Oversleep = 4 * Ms;
    const int64 Late = Clock.Frame(Limiter, 0);
    REQUIRE(Late == Start + Period + 3 * Ms + VirtualFrames::SpinReadNs);
    Clock.Oversleep = 0;
    REQUIRE(Clock.Frame(Limiter, 0) == Start + 2 * Period);

    // A frame that works 2.5 periods (a hitch): no wait now, and the schedule starts again
    // from the end of the hitch instead of running frames back to back
    const int64 HitchEnd = Clock.Frame(Limiter, 5 * Period / 2);
    REQUIRE(Limiter.GetRestarts() == 1);
    REQUIRE(Clock.Frame(Limiter, 0) == HitchEnd + Period);
}

TEST_CASE("Frame limiter: a new cap takes effect at the next frame; the same cap changes nothing", "[FrameLimiter]")
{
    VirtualFrames Clock;
    FrameLimiter Limiter = Clock.Make(1 * Ms);
    Limiter.SetMaxFps(60);
    REQUIRE(Limiter.GetMaxFps() == 60);
    const int64 Period60 = Limiter.GetPeriodNs();
    const int64 Start = Clock.Frame(Limiter, 0);
    REQUIRE(Clock.Frame(Limiter, 0) == Start + Period60);

    // The engine sets the limiter every frame: the same cap keeps the schedule
    Limiter.SetMaxFps(60);
    REQUIRE(Clock.Frame(Limiter, 0) == Start + 2 * Period60);
    REQUIRE(Limiter.GetRestarts() == 0);

    // A new cap: the next wait starts the schedule from now, then frames are 1/120 s apart
    Limiter.SetMaxFps(120);
    REQUIRE(Limiter.GetMaxFps() == 120);
    const int64 Period120 = Limiter.GetPeriodNs();
    REQUIRE(Period120 == 8'333'333);
    const int64 Restart = Clock.Frame(Limiter, 1 * Ms);
    REQUIRE(Restart == Start + 2 * Period60 + 1 * Ms);
    for (int i = 1; i <= 10; ++i)
    {
        const int64 Begin = Clock.Frame(Limiter, 0);
        REQUIRE(Begin >= Restart + i * Period120);
        REQUIRE(Begin <= Restart + i * Period120 + VirtualFrames::SpinReadNs);
    }

    // No cap: no wait at all
    Limiter.SetMaxFps(0);
    const int64 Before = Clock.Now;
    REQUIRE(Clock.Frame(Limiter, 0) == Before);
}

TEST_CASE("Frame limiter: no cap never waits or reads the clock", "[FrameLimiter]")
{
    VirtualFrames Clock;
    FrameLimiter Limiter = Clock.Make(1 * Ms);
    Limiter.SetMaxFps(0);
    REQUIRE(Limiter.GetPeriodNs() == 0);
    for (int i = 0; i < 10; ++i)
    {
        Clock.Frame(Limiter, 0);
    }
    REQUIRE(Clock.Reads == 0);
    REQUIRE(Clock.Sleeps == 0);

    // A new cap starts its own schedule at the next wait
    Limiter.SetMaxFps(50);
    const int64 Start = Clock.Frame(Limiter, 0);
    REQUIRE(Clock.Frame(Limiter, 0) == Start + Limiter.GetPeriodNs());
}

TEST_CASE("Frame limiter: the engine's limiter spins at most 1 ms, and at most half a period", "[FrameLimiter]")
{
    FrameLimiter Limiter;
    REQUIRE(Limiter.GetPeriodNs() == 0);
    Limiter.SetMaxFps(30);
    REQUIRE(Limiter.GetSpinNs() == FrameLimiter::DefaultSpinNs);
    REQUIRE(FrameLimiter::DefaultSpinNs == 1 * Ms);
    Limiter.SetMaxFps(1000);
    REQUIRE(Limiter.GetSpinNs() == Limiter.GetPeriodNs() / 2);
}

TEST_CASE("Sleep timer: it never wakes before the deadline", "[FrameLimiter]")
{
    using Clock = std::chrono::steady_clock;
    SleepTimer Timer;
    for (const int64 WaitNs : {0 * Ms, 1 * Ms, 3 * Ms})
    {
        const Clock::time_point Deadline = Clock::now() + std::chrono::nanoseconds(WaitNs);
        Timer.SleepUntil(Deadline);
        REQUIRE(Clock::now() >= Deadline);
    }
    // A deadline in the past returns at once
    Timer.SleepUntil(Clock::now() - std::chrono::seconds(1));
}

TEST_CASE("Frame limiter: WillWait says if the next wait would sleep", "[FrameLimiter]")
{
    VirtualFrames Clock;
    FrameLimiter Limiter = Clock.Make(1 * Ms);

    // No cap: never
    REQUIRE_FALSE(Limiter.WillWait());

    Limiter.SetMaxFps(100);
    const int64 Period = Limiter.GetPeriodNs();
    // The schedule has not started: the first wait does not sleep
    REQUIRE_FALSE(Limiter.WillWait());
    Clock.Frame(Limiter, 0);

    // A frame that is faster than the period will wait, a slower one will not
    Clock.Now += Period / 2;
    REQUIRE(Limiter.WillWait());
    Clock.Now += Period;
    REQUIRE_FALSE(Limiter.WillWait());
}
