#include "Threading/CommandRunnable.h"
#include "Threading/Thread.h"
#include "Threading/TickLimiter.h"

#include <algorithm>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <latch>
#include <memory>
#include <thread>
#include <utility>
#include <vector>

using namespace Fling;
using namespace std::chrono_literals;

// A wake is never early, so the lower bounds here are exact. A wake can be late
// on a busy CI machine, so the upper bounds are large.

namespace
{
    using Clock = std::chrono::steady_clock;

    /** Counts ticks, and opens Done at the tick t_TickCount. */
    class FixedRateRunnable : public CommandRunnable
    {
    public:
        FixedRateRunnable(TickRate t_Rate, int t_TickCount) : CommandRunnable(t_Rate), m_TickCount(t_TickCount) {}

        void Tick() override
        {
            if (++Ticks == m_TickCount)
            {
                Done.count_down();
            }
        }

        std::atomic<int> Ticks{0};
        std::latch Done{1};

    private:
        const int m_TickCount;
    };
} // namespace

TEST_CASE("TickRate from Hz", "[Threading][TickRate]")
{
    REQUIRE(TickRate::Hz(60).IsFixed());
    REQUIRE(TickRate::Hz(60).Period == std::chrono::nanoseconds(16'666'666));
    REQUIRE(TickRate::Hz(120).Period == std::chrono::nanoseconds(8'333'333));
    REQUIRE(TickRate::Hz(1).Period == 1s);
    REQUIRE_FALSE(TickRate::Hz(0).IsFixed());
    REQUIRE_FALSE(TickRate::Hz(-5).IsFixed());
    REQUIRE_FALSE(TickRate{}.IsFixed());
}

TEST_CASE("TickLimiter ticks are never early", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(200);
    TickLimiter Limiter(Rate);
    REQUIRE(Limiter.GetRate().Period == Rate.Period);

    constexpr int TickCount = 20;
    const Clock::time_point Start = Clock::now();
    for (int Tick = 1; Tick <= TickCount; ++Tick)
    {
        REQUIRE(Limiter.WaitForNextTick());
        REQUIRE(Clock::now() - Start >= Rate.Period * Tick);
    }
    REQUIRE(Clock::now() - Start < Rate.Period * TickCount + 2s);
    REQUIRE(Limiter.GetMissedTicks() == 0);
}

TEST_CASE("TickLimiter keeps its schedule after a slightly late tick", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(100);
    TickLimiter Limiter(Rate);

    const Clock::time_point Start = Clock::now();
    REQUIRE(Limiter.WaitForNextTick());

    // The Tick takes until a quarter period after tick 2 is due: tick 2 runs at once
    std::this_thread::sleep_until(Start + Rate.Period * 2 + Rate.Period / 4);
    const Clock::time_point LateCall = Clock::now();
    REQUIRE(Limiter.WaitForNextTick());
    REQUIRE(Clock::now() - LateCall < Rate.Period);

    // Tick 2 started less than half a period late, so tick 3 stays on the old schedule
    REQUIRE(Limiter.WaitForNextTick());
    REQUIRE(Clock::now() - Start >= Rate.Period * 3);
    REQUIRE(Limiter.GetMissedTicks() == 0);
}

TEST_CASE("TickLimiter starts the schedule again after a very late tick", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(100);
    TickLimiter Limiter(Rate);

    const Clock::time_point Start = Clock::now();
    REQUIRE(Limiter.WaitForNextTick());

    // Tick 2 starts three quarters of a period late
    std::this_thread::sleep_until(Start + Rate.Period * 2 + Rate.Period * 3 / 4);
    REQUIRE(Limiter.WaitForNextTick());
    const Clock::time_point LateReturn = Clock::now();

    // Tick 3 is one period after tick 2, not back to back with it
    REQUIRE(Limiter.WaitForNextTick());
    REQUIRE(Clock::now() - LateReturn >= Rate.Period / 2);
    REQUIRE(Limiter.GetMissedTicks() >= 1);
}

TEST_CASE("TickLimiter drops ticks when a Tick takes too long", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(1000);
    TickLimiter Limiter(Rate);

    REQUIRE(Limiter.WaitForNextTick());
    std::this_thread::sleep_for(20ms);

    // The tick that is due runs at once
    const Clock::time_point Before = Clock::now();
    REQUIRE(Limiter.WaitForNextTick());
    REQUIRE(Clock::now() - Before < 1s);

    // That tick started about 19 periods late, so the schedule starts again
    // from it and the old ticks are counted. The next tick is not a burst.
    const Clock::time_point LateReturn = Clock::now();
    REQUIRE(Limiter.WaitForNextTick());
    REQUIRE(Limiter.GetMissedTicks() >= 10);
    REQUIRE(Clock::now() - LateReturn >= Rate.Period / 2);
}

TEST_CASE("Precise spin window is capped at half a period", "[Threading][TickRate]")
{
    TickRate Fast = TickRate::Hz(1000, TickPrecision::Precise);
    REQUIRE(TickLimiter(Fast).GetSpinMargin() <= Fast.Period / 2);

    TickRate Slow = TickRate::Hz(10, TickPrecision::Precise);
    Slow.SpinWindow = 3ms;
    REQUIRE(TickLimiter(Slow).GetSpinMargin() >= 3ms);
    REQUIRE(TickLimiter(Slow).GetSpinMargin() <= Slow.Period / 2);

    TickRate Huge = TickRate::Hz(100, TickPrecision::Precise);
    Huge.SpinWindow = 1s;
    REQUIRE(TickLimiter(Huge).GetSpinMargin() == Huge.Period / 2);
}

TEST_CASE("TickLimiter Interrupt stops a wait", "[Threading][TickRate]")
{
    TickLimiter Limiter(TickRate::Hz(1));

    std::latch Waiting(1);
    bool Result = true;
    Clock::duration Waited{};
    std::shared_ptr<Thread> Waiter = Thread::Create({.Name = "Tick waiter"},
                                                    [&]()
                                                    {
                                                        Waiting.count_down();
                                                        const Clock::time_point Before = Clock::now();
                                                        Result = Limiter.WaitForNextTick();
                                                        Waited = Clock::now() - Before;
                                                    });
    REQUIRE(Waiter);

    Waiting.wait();
    Limiter.Interrupt();
    Waiter->Join();

    REQUIRE_FALSE(Result);
    REQUIRE(Waited < 900ms);
    REQUIRE(Limiter.IsInterrupted());

    // An interrupt stays set
    REQUIRE_FALSE(Limiter.WaitForNextTick());
}

TEST_CASE("TickLimiter with no fixed rate does not wait", "[Threading][TickRate]")
{
    TickLimiter Limiter(TickRate{});
    const Clock::time_point Start = Clock::now();
    for (int Tick = 0; Tick < 1000; ++Tick)
    {
        REQUIRE(Limiter.WaitForNextTick());
    }
    REQUIRE(Clock::now() - Start < 1s);

    Limiter.Interrupt();
    REQUIRE_FALSE(Limiter.WaitForNextTick());
}

TEST_CASE("CommandRunnable at a fixed rate", "[Threading][TickRate]")
{
    SECTION("Ticks are at the rate, and commands run on the thread")
    {
        const TickRate Rate = TickRate::Hz(100);
        constexpr int TickCount = 10;
        auto Work = std::make_shared<FixedRateRunnable>(Rate, TickCount);
        REQUIRE(Work->GetTickRate().Period == Rate.Period);

        const Clock::time_point Start = Clock::now();
        std::shared_ptr<Thread> Worker = Thread::Create(Work, {.Name = "Fixed rate"});
        REQUIRE(Worker);

        std::atomic<ThreadId> RanOn{InvalidThreadId};
        Work->Enqueue([&RanOn]() { RanOn = Thread::CurrentId(); });

        Work->Done.wait();
        REQUIRE(Clock::now() - Start >= Rate.Period * TickCount);

        Worker->Kill();
        REQUIRE(RanOn == Worker->GetId());
    }

    SECTION("Kill does not wait for the next tick, and commands still run")
    {
        auto Work = std::make_shared<FixedRateRunnable>(TickRate::Hz(1), 1000);
        std::shared_ptr<Thread> Worker = Thread::Create(Work, {.Name = "Slow rate"});
        REQUIRE(Worker);

        std::atomic<int> Ran{0};
        for (int Index = 0; Index < 10; ++Index)
        {
            Work->Enqueue([&Ran]() { ++Ran; });
        }

        const Clock::time_point Before = Clock::now();
        Worker->Kill();
        REQUIRE(Clock::now() - Before < 900ms);
        REQUIRE(Ran == 10);
    }

    SECTION("A TickRate that is not fixed gives the wake interval mode")
    {
        FixedRateRunnable Work(TickRate{}, 1);
        REQUIRE_FALSE(Work.GetTickRate().IsFixed());
        REQUIRE(Work.GetMissedTicks() == 0);
    }
}

TEST_CASE("Thread::Create with a tick rate", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(200);
    constexpr int TickCount = 5;
    std::atomic<int> Ticks{0};
    std::latch Done(1);

    const Clock::time_point Start = Clock::now();
    std::shared_ptr<Thread> Worker = Thread::Create({.Name = "Ticking lambda"},
                                                    Rate,
                                                    [&]()
                                                    {
                                                        if (++Ticks == TickCount)
                                                        {
                                                            Done.count_down();
                                                        }
                                                    });
    REQUIRE(Worker);

    Done.wait();
    REQUIRE(Clock::now() - Start >= Rate.Period * TickCount);
    Worker->Kill();
    REQUIRE_FALSE(Worker->IsRunning());
}

TEST_CASE("TickRate precision", "[Threading][TickRate]")
{
    REQUIRE_FALSE(TickRate::Hz(60).IsPrecise());
    REQUIRE(TickRate::Hz(60, TickPrecision::Precise).IsPrecise());
    REQUIRE(TickRate::Hz(60, TickPrecision::Precise).Period == TickRate::Hz(60).Period);
    REQUIRE_FALSE(TickRate::Hz(0, TickPrecision::Precise).IsFixed());

    REQUIRE(TickLimiter(TickRate::Hz(60)).GetSpinMargin() == std::chrono::nanoseconds(0));
    REQUIRE(TickLimiter(TickRate::Hz(60, TickPrecision::Precise)).GetSpinMargin() > std::chrono::nanoseconds(0));
    REQUIRE(TickLimiter(TickRate{}).GetSpinMargin() == std::chrono::nanoseconds(0));
}

TEST_CASE("Precise TickLimiter ticks are never early", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(200, TickPrecision::Precise);
    TickLimiter Limiter(Rate);

    constexpr int TickCount = 20;
    const Clock::time_point Start = Clock::now();
    for (int Tick = 1; Tick <= TickCount; ++Tick)
    {
        REQUIRE(Limiter.WaitForNextTick());
        REQUIRE(Clock::now() - Start >= Rate.Period * Tick);
    }
    REQUIRE(Clock::now() - Start < Rate.Period * TickCount + 2s);
}

TEST_CASE("Precise TickLimiter Interrupt stops a wait", "[Threading][TickRate]")
{
    TickLimiter Limiter(TickRate::Hz(1, TickPrecision::Precise));

    std::latch Waiting(1);
    bool Result = true;
    Clock::duration Waited{};
    std::shared_ptr<Thread> Waiter = Thread::Create({.Name = "Precise waiter"},
                                                    [&]()
                                                    {
                                                        Waiting.count_down();
                                                        const Clock::time_point Before = Clock::now();
                                                        Result = Limiter.WaitForNextTick();
                                                        Waited = Clock::now() - Before;
                                                    });
    REQUIRE(Waiter);

    Waiting.wait();
    Limiter.Interrupt();
    Waiter->Join();

    REQUIRE_FALSE(Result);
    REQUIRE(Waited < 900ms);
}

TEST_CASE("Thread::Create with a precise tick rate", "[Threading][TickRate]")
{
    const TickRate Rate = TickRate::Hz(200, TickPrecision::Precise);
    constexpr int TickCount = 5;
    std::atomic<int> Ticks{0};
    std::latch Done(1);

    const Clock::time_point Start = Clock::now();
    std::shared_ptr<Thread> Worker = Thread::Create({.Name = "Precise lambda"},
                                                    Rate,
                                                    [&]()
                                                    {
                                                        if (++Ticks == TickCount)
                                                        {
                                                            Done.count_down();
                                                        }
                                                    });
    REQUIRE(Worker);

    Done.wait();
    REQUIRE(Clock::now() - Start >= Rate.Period * TickCount);
    Worker->Kill();
}

// Hidden: timing depends on the machine. Run it with
//   ./build/FlingTests/bin/CoreTests "[TickPrecision]" -s
TEST_CASE("TickLimiter wake accuracy", "[.][TickPrecision]")
{
    auto Measure = [](TickPrecision t_Precision)
    {
        const TickRate Rate = TickRate::Hz(100, t_Precision);
        TickLimiter Limiter(Rate);
        constexpr int TickCount = 100;

        std::vector<std::chrono::nanoseconds> Late;
        const Clock::time_point Start = Clock::now();
        for (int Tick = 1; Tick <= TickCount; ++Tick)
        {
            REQUIRE(Limiter.WaitForNextTick());
            // The first call starts the schedule a little after Start, so this is an upper bound
            Late.push_back(std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - Start) -
                           Rate.Period * Tick);
        }
        std::sort(Late.begin(), Late.end());
        return std::pair{Late[Late.size() / 2], Late.back()};
    };

    const auto [NormalMedian, NormalMax] = Measure(TickPrecision::Normal);
    const auto [PreciseMedian, PreciseMax] = Measure(TickPrecision::Precise);
    WARN("Normal  late: median " << NormalMedian.count() / 1000 << " us, max " << NormalMax.count() / 1000 << " us");
    WARN("Precise late: median " << PreciseMedian.count() / 1000 << " us, max " << PreciseMax.count() / 1000 << " us");
    REQUIRE(PreciseMedian <= NormalMedian);
}
