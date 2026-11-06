#include "Threading/ConditionVariable.h"
#include "Threading/Event.h"
#include "Threading/Mutex.h"
#include "Threading/Semaphore.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

using namespace Fling;

namespace
{
    template <typename T_Mutex, typename T_Lock>
    int CountIncrements()
    {
        constexpr int ThreadCount = 8;
        constexpr int Iters = 100000;

        // A member, so FLING_GUARDED_BY is checked. The attribute does not apply to a local.
        struct Box
        {
            T_Mutex Mu;
            int Counter FLING_GUARDED_BY(Mu) = 0;
        };
        Box State;

        std::latch Start(ThreadCount);
        std::vector<std::thread> Workers;
        Workers.reserve(ThreadCount);
        for (int Index = 0; Index < ThreadCount; ++Index)
        {
            Workers.emplace_back(
                [&]()
                {
                    Start.arrive_and_wait();
                    for (int Step = 0; Step < Iters; ++Step)
                    {
                        T_Lock Lock(State.Mu);
                        ++State.Counter;
                    }
                });
        }
        for (std::thread& Worker : Workers)
        {
            Worker.join();
        }

        T_Lock Lock(State.Mu);
        return State.Counter;
    }

    template <typename T_Mutex, typename T_Lock>
    bool TryLockFailsWhileHeld()
    {
        T_Mutex Mu;
        T_Lock Held(Mu);
        std::atomic<bool> Failed{false};
        std::thread Worker([&]() { Failed.store(!Mu.TryLock()); });
        Worker.join();
        return Failed.load();
    }
} // namespace

TEST_CASE("Mutex counts increments and TryLock fails while held", "[Threading]")
{
    REQUIRE(CountIncrements<Mutex, ScopedLock>() == 8 * 100000);
    REQUIRE(TryLockFailsWhileHeld<Mutex, ScopedLock>());
}

TEST_CASE("ScopedLock can unlock early and lock again", "[Threading]")
{
    struct Box
    {
        Mutex Mu;
        int Counter FLING_GUARDED_BY(Mu) = 0;
    };
    Box State;
    int Seen = 0;

    {
        ScopedLock Lock(State.Mu);
        ++State.Counter;
        Lock.Unlock();

        std::thread Worker(
            [&]()
            {
                ScopedLock Inner(State.Mu);
                Seen = State.Counter;
                ++State.Counter;
            });
        Worker.join();

        Lock.Lock();
        REQUIRE(State.Counter == 2);
    }

    REQUIRE(Seen == 1);
}

TEST_CASE("SharedMutex lets two readers in and keeps a writer out", "[Threading]")
{
    SharedMutex Mu;
    std::atomic<int> Inside{0};
    std::latch BothInside(2);
    std::latch ReleaseReaders(1);

    auto Reader = [&]()
    {
        ReadScopedLock Lock(Mu);
        Inside.fetch_add(1);
        BothInside.count_down();
        ReleaseReaders.wait();
    };

    std::thread First(Reader);
    std::thread Second(Reader);
    BothInside.wait();
    REQUIRE(Inside.load() == 2);
    if (Mu.TryLockShared())
    {
        Mu.UnlockShared();
    }
    else
    {
        REQUIRE(false);
    }
    REQUIRE_FALSE(Mu.TryLock());

    std::atomic<bool> WriterEntered{false};
    std::thread Writer(
        [&]()
        {
            WriteScopedLock Lock(Mu);
            WriterEntered.store(true);
        });

    ReleaseReaders.count_down();
    First.join();
    Second.join();
    Writer.join();
    REQUIRE(WriterEntered.load());
}

TEST_CASE("SpinLock counts increments and TryLock fails while held", "[Threading]")
{
    REQUIRE(CountIncrements<SpinLock, ScopedSpinLock>() == 8 * 100000);
    REQUIRE(TryLockFailsWhileHeld<SpinLock, ScopedSpinLock>());
}

TEST_CASE("Condition variable predicate wait wakes on notify", "[Threading]")
{
    struct Box
    {
        Mutex Mu;
        ConditionVariable Cv;
        bool Ready FLING_GUARDED_BY(Mu) = false;
    };
    Box State;
    std::latch Holding(1);
    std::latch Done(1);

    std::thread Worker(
        [&]()
        {
            ScopedLock Lock(State.Mu);
            Holding.count_down();
            State.Cv.Wait(Lock, [&]() FLING_REQUIRES(State.Mu) { return State.Ready; });
            Done.count_down();
        });

    Holding.wait();
    {
        ScopedLock Lock(State.Mu);
        State.Ready = true;
    }
    State.Cv.NotifyOne();
    Done.wait();
    Worker.join();
}

TEST_CASE("Condition variable WaitFor times out", "[Threading]")
{
    struct Box
    {
        Mutex Mu;
        ConditionVariable Cv;
        bool Ready FLING_GUARDED_BY(Mu) = false;
    };
    Box State;
    ScopedLock Lock(State.Mu);
    const bool Woke =
        State.Cv.WaitFor(Lock, std::chrono::milliseconds(1), [&]() FLING_REQUIRES(State.Mu) { return State.Ready; });
    REQUIRE_FALSE(Woke);
}

TEST_CASE("Semaphore gives out its count", "[Threading]")
{
    Semaphore Sem(2);
    REQUIRE(Sem.TryAcquire());
    REQUIRE(Sem.TryAcquire());
    REQUIRE_FALSE(Sem.TryAcquire());
    REQUIRE_FALSE(Sem.TryAcquireFor(std::chrono::milliseconds(1)));

    Sem.Release(3);
    REQUIRE(Sem.TryAcquire());
    REQUIRE(Sem.TryAcquire());
    REQUIRE(Sem.TryAcquire());
    REQUIRE_FALSE(Sem.TryAcquire());

    std::latch Done(1);
    std::thread Worker(
        [&]()
        {
            Sem.Acquire();
            Done.count_down();
        });
    Sem.Release();
    Done.wait();
    Worker.join();
    REQUIRE_FALSE(Sem.TryAcquire());
}

TEST_CASE("Event auto reset lets one waiter through per signal", "[Threading]")
{
    Event Ev(EventReset::Auto);
    std::latch Ready(2);
    std::latch FirstDone(1);
    std::atomic<int> Passed{0};

    auto Waiter = [&]()
    {
        Ready.count_down();
        Ev.Wait();
        if (Passed.fetch_add(1) == 0)
        {
            FirstDone.count_down();
        }
    };

    std::thread First(Waiter);
    std::thread Second(Waiter);
    Ready.wait();
    Ev.Signal();
    FirstDone.wait();
    const int AfterOne = Passed.load();
    Ev.Signal();
    First.join();
    Second.join();

    REQUIRE(AfterOne == 1);
    REQUIRE(Passed.load() == 2);
    REQUIRE_FALSE(Ev.WaitFor(std::chrono::milliseconds(1)));
}

TEST_CASE("Event manual reset stays set until Reset", "[Threading]")
{
    Event Ev(EventReset::Manual);
    std::latch Ready(2);
    std::latch Done(2);
    std::atomic<int> Passed{0};

    auto Waiter = [&]()
    {
        Ready.count_down();
        Ev.Wait();
        Passed.fetch_add(1);
        Done.count_down();
    };

    std::thread First(Waiter);
    std::thread Second(Waiter);
    Ready.wait();
    Ev.Signal();
    Done.wait();
    First.join();
    Second.join();
    REQUIRE(Passed.load() == 2);

    Ev.Reset();
    REQUIRE_FALSE(Ev.IsSet());
    REQUIRE_FALSE(Ev.WaitFor(std::chrono::milliseconds(1)));

    Ev.Signal();
    REQUIRE(Ev.IsSet());
    Ev.Wait();
    REQUIRE(Ev.IsSet());
}

TEST_CASE("Event that starts set does not block", "[Threading]")
{
    Event Auto(EventReset::Auto, true);
    REQUIRE(Auto.IsSet());
    Auto.Wait();
    REQUIRE_FALSE(Auto.IsSet());

    Event Manual(EventReset::Manual, true);
    Manual.Wait();
    Manual.Wait();
    REQUIRE(Manual.IsSet());
}
