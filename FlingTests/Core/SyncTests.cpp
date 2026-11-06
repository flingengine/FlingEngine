#include "Threading/ConditionVariable.h"
#include "Threading/CpuRelax.h"
#include "Threading/Event.h"
#include "Threading/Mutex.h"
#include "Threading/Semaphore.h"
#include "Threading/ThreadSafety.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <latch>
#include <memory>
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

    /** Take and release a shared lock. try_lock_shared can fail spuriously, so try more than once. */
    bool CanTakeShared(SharedMutex& t_Mu)
    {
        for (int Attempt = 0; Attempt < 1000; ++Attempt)
        {
            if (t_Mu.TryLockShared())
            {
                t_Mu.UnlockShared();
                return true;
            }
            CpuRelax();
        }
        return false;
    }

    /** Take and release the exclusive lock, if TryLock gets it. */
    bool CanTakeExclusive(SharedMutex& t_Mu)
    {
        if (t_Mu.TryLock())
        {
            t_Mu.Unlock();
            return true;
        }
        return false;
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

    // Join every thread before a REQUIRE. A failed REQUIRE throws, and a
    // joinable std::thread that is destroyed calls std::terminate.
    const int InsideCount = Inside.load();
    const bool ReadTaken = CanTakeShared(Mu);
    const bool WriteTaken = CanTakeExclusive(Mu);

    std::atomic<bool> WriterEntered{false};
    std::thread Writer(
        [&]()
        {
            WriteScopedLock Lock(Mu);
            WriterEntered.store(true);
        });

    // Give the writer time to reach the lock. It must stay out while the readers are in.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    const bool EnteredWithReaders = WriterEntered.load();

    ReleaseReaders.count_down();
    First.join();
    Second.join();
    Writer.join();

    REQUIRE(InsideCount == 2);
    REQUIRE(ReadTaken);
    REQUIRE_FALSE(WriteTaken);
    REQUIRE_FALSE(EnteredWithReaders);
    REQUIRE(WriterEntered.load());
}

TEST_CASE("SharedMutex lets one writer in at a time", "[Threading]")
{
    REQUIRE(CountIncrements<SharedMutex, WriteScopedLock>() == 8 * 100000);
    REQUIRE(TryLockFailsWhileHeld<SharedMutex, WriteScopedLock>());
}

TEST_CASE("SharedMutex writer keeps readers out", "[Threading]")
{
    SharedMutex Mu;
    std::atomic<bool> ReaderEntered{false};
    std::thread Reader;
    bool EnteredWithWriter = false;
    bool ReadTakenWithWriter = false;
    {
        WriteScopedLock Lock(Mu);
        Reader = std::thread(
            [&]()
            {
                ReadScopedLock Read(Mu);
                ReaderEntered.store(true);
            });

        std::thread Probe([&]() { ReadTakenWithWriter = CanTakeShared(Mu); });
        Probe.join();

        // Give the reader time to reach the lock. It must stay out while the writer is in.
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        EnteredWithWriter = ReaderEntered.load();
    }
    Reader.join();

    REQUIRE_FALSE(ReadTakenWithWriter);
    REQUIRE_FALSE(EnteredWithWriter);
    REQUIRE(ReaderEntered.load());
}

TEST_CASE("ReadScopedLock can unlock early and lock again", "[Threading]")
{
    SharedMutex Mu;
    bool WriteTakenWhileUnlocked = false;
    bool WriteTakenWhileLocked = true;
    {
        ReadScopedLock Lock(Mu);
        Lock.Unlock();
        std::thread Writer([&]() { WriteTakenWhileUnlocked = CanTakeExclusive(Mu); });
        Writer.join();

        Lock.Lock();
        std::thread SecondWriter([&]() { WriteTakenWhileLocked = CanTakeExclusive(Mu); });
        SecondWriter.join();
    }

    REQUIRE(WriteTakenWhileUnlocked);
    REQUIRE_FALSE(WriteTakenWhileLocked);
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

TEST_CASE("Condition variable WaitFor with a very long timeout waits for the predicate", "[Threading]")
{
    struct Box
    {
        Mutex Mu;
        ConditionVariable Cv;
        bool Ready FLING_GUARDED_BY(Mu) = false;
    };
    Box State;

    std::thread Worker(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ScopedLock Lock(State.Mu);
            State.Ready = true;
            State.Cv.NotifyOne();
        });

    bool Woke = false;
    {
        ScopedLock Lock(State.Mu);
        Woke =
            State.Cv.WaitFor(Lock, std::chrono::hours::max(), [&]() FLING_REQUIRES(State.Mu) { return State.Ready; });
    }
    Worker.join();
    REQUIRE(Woke);
}

TEST_CASE("Condition variable NotifyAll wakes every waiter", "[Threading]")
{
    constexpr int WaiterCount = 4;
    struct Box
    {
        Mutex Mu;
        ConditionVariable Cv;
        bool Ready FLING_GUARDED_BY(Mu) = false;
        int Waiting FLING_GUARDED_BY(Mu) = 0;
    };
    Box State;
    std::atomic<int> Woke{0};

    std::vector<std::thread> Waiters;
    for (int Index = 0; Index < WaiterCount; ++Index)
    {
        Waiters.emplace_back(
            [&]()
            {
                ScopedLock Lock(State.Mu);
                ++State.Waiting;
                State.Cv.NotifyAll();
                State.Cv.Wait(Lock, [&]() FLING_REQUIRES(State.Mu) { return State.Ready; });
                Woke.fetch_add(1);
            });
    }

    {
        // Wait until every waiter is in the wait, then wake them with one call.
        ScopedLock Lock(State.Mu);
        State.Cv.Wait(Lock, [&]() FLING_REQUIRES(State.Mu) { return State.Waiting == WaiterCount; });
        State.Ready = true;
        State.Cv.NotifyAll();
    }
    for (std::thread& Waiter : Waiters)
    {
        Waiter.join();
    }
    REQUIRE(Woke.load() == WaiterCount);
}

TEST_CASE("Condition variable Wait with no predicate wakes on notify", "[Threading]")
{
    struct Box
    {
        Mutex Mu;
        ConditionVariable Cv;
        bool Ready FLING_GUARDED_BY(Mu) = false;
    };
    Box State;

    std::thread Worker(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ScopedLock Lock(State.Mu);
            State.Ready = true;
            State.Cv.NotifyOne();
        });

    bool Seen = false;
    {
        ScopedLock Lock(State.Mu);
        // A wait with no predicate can wake spuriously, so loop on the flag.
        while (!State.Ready)
        {
            State.Cv.Wait(Lock);
        }
        Seen = State.Ready;
    }
    Worker.join();
    REQUIRE(Seen);
}

TEST_CASE("Condition variable WaitUntil times out and wakes on the predicate", "[Threading]")
{
    struct Box
    {
        Mutex Mu;
        ConditionVariable Cv;
        bool Ready FLING_GUARDED_BY(Mu) = false;
    };
    Box State;
    const auto Pred = [&]() FLING_REQUIRES(State.Mu)
    {
        return State.Ready;
    };

    bool TimedOut = false;
    {
        ScopedLock Lock(State.Mu);
        TimedOut = !State.Cv.WaitUntil(Lock, std::chrono::steady_clock::now() + std::chrono::milliseconds(1), Pred);
    }

    std::thread Worker(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            ScopedLock Lock(State.Mu);
            State.Ready = true;
            State.Cv.NotifyOne();
        });

    bool Woke = false;
    {
        ScopedLock Lock(State.Mu);
        Woke = State.Cv.WaitUntil(Lock, std::chrono::steady_clock::now() + std::chrono::seconds(30), Pred);
    }
    Worker.join();

    REQUIRE(TimedOut);
    REQUIRE(Woke);
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

    std::thread Releaser(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Sem.Release();
        });
    const bool Acquired = Sem.TryAcquireFor(std::chrono::nanoseconds::max());
    Releaser.join();
    REQUIRE(Acquired);
}

TEST_CASE("Semaphore keeps the number of holders at its count", "[Threading]")
{
    constexpr int Permits = 3;
    constexpr int ThreadCount = 8;
    constexpr int Iters = 2000;
    Semaphore Sem(Permits);
    std::atomic<int> Inside{0};
    std::atomic<int> MaxInside{0};

    std::latch Start(ThreadCount);
    std::vector<std::thread> Workers;
    for (int Index = 0; Index < ThreadCount; ++Index)
    {
        Workers.emplace_back(
            [&]()
            {
                Start.arrive_and_wait();
                for (int Step = 0; Step < Iters; ++Step)
                {
                    Sem.Acquire();
                    const int Now = Inside.fetch_add(1) + 1;
                    int Seen = MaxInside.load();
                    while (Now > Seen && !MaxInside.compare_exchange_weak(Seen, Now))
                    {
                    }
                    Inside.fetch_sub(1);
                    Sem.Release();
                }
            });
    }
    for (std::thread& Worker : Workers)
    {
        Worker.join();
    }

    REQUIRE(MaxInside.load() <= Permits);
    REQUIRE(MaxInside.load() >= 1);
    for (int Index = 0; Index < Permits; ++Index)
    {
        REQUIRE(Sem.TryAcquire());
    }
    REQUIRE_FALSE(Sem.TryAcquire());
}

TEST_CASE("Semaphore passes every permit from many producers to many consumers", "[Threading]")
{
    constexpr int Producers = 4;
    constexpr int Consumers = 4;
    constexpr int PerThread = 5000;
    Semaphore Sem;
    std::atomic<int> Consumed{0};

    std::vector<std::thread> Threads;
    for (int Index = 0; Index < Consumers; ++Index)
    {
        Threads.emplace_back(
            [&]()
            {
                for (int Step = 0; Step < PerThread; ++Step)
                {
                    Sem.Acquire();
                    Consumed.fetch_add(1);
                }
            });
    }
    for (int Index = 0; Index < Producers; ++Index)
    {
        Threads.emplace_back(
            [&]()
            {
                for (int Step = 0; Step < PerThread; ++Step)
                {
                    Sem.Release();
                }
            });
    }
    for (std::thread& Thread : Threads)
    {
        Thread.join();
    }

    REQUIRE(Consumed.load() == Producers * PerThread);
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
    // Give a second waiter time to pass by mistake on the same signal.
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
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

TEST_CASE("Event WaitFor with a very long timeout waits for the signal", "[Threading]")
{
    Event Ev(EventReset::Auto);
    std::thread Signaler(
        [&]()
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            Ev.Signal();
        });
    const bool Signaled = Ev.WaitFor(std::chrono::hours::max());
    Signaler.join();
    REQUIRE(Signaled);
}

TEST_CASE("Event can be destroyed as soon as its wait returns", "[Threading]")
{
    // A regression test for Signal: it must be done with the Event when the
    // waiter wakes. Run it under a sanitizer to see a use after free.
    for (int Round = 0; Round < 500; ++Round)
    {
        auto Ev = std::make_unique<Event>(EventReset::Manual);
        Event* Raw = Ev.get();
        std::thread Signaler([Raw]() { Raw->Signal(); });
        Ev->Wait();
        Ev.reset();
        Signaler.join();
    }
    SUCCEED();
}
