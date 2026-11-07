#include "Strings/String.h"
#include "Threading/CommandQueue.h"
#include "Threading/CommandRunnable.h"
#include "Threading/Thread.h"
#include "Threading/ThreadManager.h"
#include "Threading/ThreadQueue.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <latch>
#include <memory>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

using Fling::String;
using Fling::StringView;

using namespace Fling;

namespace
{
    /** Records the order of the Runnable calls. */
    class RecordingRunnable : public Runnable
    {
    public:
        explicit RecordingRunnable(bool t_InitResult = true) : m_InitResult(t_InitResult) {}

        bool Init() override
        {
            Record("Init");
            return m_InitResult;
        }

        uint32 Run() override
        {
            Record("Run");
            while (!m_Stop.load())
            {
                std::this_thread::yield();
            }
            return 0;
        }

        void Stop() override { m_Stop.store(true); }

        void Exit() override { Record("Exit"); }

        std::vector<String> Calls()
        {
            std::lock_guard<std::mutex> Lock(m_Mutex);
            return m_Calls;
        }

    private:
        void Record(const char* t_Call)
        {
            std::lock_guard<std::mutex> Lock(m_Mutex);
            m_Calls.emplace_back(t_Call);
        }

        const bool m_InitResult;
        std::atomic<bool> m_Stop{false};
        std::mutex m_Mutex;
        std::vector<String> m_Calls;
    };

    /** Counts its Tick calls. */
    class TickingRunnable : public CommandRunnable
    {
    public:
        TickingRunnable() : CommandRunnable(std::chrono::milliseconds(1)) {}

        void Tick() override { ++Ticks; }

        std::atomic<int> Ticks{0};
    };

    struct PlaySoundRequest
    {
        uint32 SoundId = 0;
        float Volume = 1.0f;
    };
} // namespace

TEST_CASE("Thread names and IDs", "[Threading]")
{
    Thread::InitGameThread("Test Game");
    REQUIRE(Thread::CurrentId() == GameThreadId);
    REQUIRE(Thread::CurrentName() == "Test Game");
    REQUIRE(Thread::IsGameThread());
    REQUIRE(ThreadManager::GetThreadName(GameThreadId) == "Test Game");

    std::latch Release(1);
    ThreadId SeenId = InvalidThreadId;
    String SeenName;
    bool SeenGame = true;

    const String LongName = "A thread name that is longer than fifteen bytes";
    std::shared_ptr<Thread> Worker = Thread::Create({.Name = LongName},
                                                    [&]()
                                                    {
                                                        SeenId = Thread::CurrentId();
                                                        SeenName = Thread::CurrentName();
                                                        SeenGame = Thread::IsGameThread();
                                                        Release.wait();
                                                    });
    REQUIRE(Worker);
    REQUIRE(Worker->IsRunning());

    const ThreadId Id = Worker->GetId();
    REQUIRE(Id != InvalidThreadId);
    REQUIRE(Id != GameThreadId);
    REQUIRE(Worker->GetName() == LongName);
    REQUIRE(Worker->GetNativeId() != 0);

    // The full name is kept in the engine, not only the 15 bytes the OS keeps
    REQUIRE(ThreadManager::GetThreadName(Id) == LongName);

    Release.count_down();
    Worker->Join();
    REQUIRE_FALSE(Worker->IsRunning());
    REQUIRE(SeenId == Id);
    REQUIRE(SeenName == LongName);
    REQUIRE_FALSE(SeenGame);
    REQUIRE(ThreadManager::GetThreadName(Id).empty());
}

TEST_CASE("Thread IDs are unique and increase", "[Threading]")
{
    std::vector<std::shared_ptr<Thread>> Threads;
    for (int Index = 0; Index < 8; ++Index)
    {
        Threads.push_back(Thread::Create({.Name = "Worker " + std::to_string(Index)}, []() {}));
        REQUIRE(Threads.back());
    }

    std::set<ThreadId> Ids;
    for (size_t Index = 0; Index < Threads.size(); ++Index)
    {
        Ids.insert(Threads[Index]->GetId());
        if (Index > 0)
        {
            REQUIRE(Threads[Index]->GetId() > Threads[Index - 1]->GetId());
        }
    }
    REQUIRE(Ids.size() == Threads.size());

    // The destructor joins
    Threads.clear();
}

TEST_CASE("Runnable lifecycle", "[Threading]")
{
    SECTION("Init, Run, then Exit, and Kill stops Run")
    {
        auto Work = std::make_shared<RecordingRunnable>();
        std::shared_ptr<Thread> Worker = Thread::Create(Work, {.Name = "Lifecycle"});
        REQUIRE(Worker);

        Worker->Kill();
        REQUIRE_FALSE(Worker->IsRunning());
        REQUIRE(Work->Calls() == std::vector<String>{"Init", "Run", "Exit"});
    }

    SECTION("Init that returns false stops the thread before Run")
    {
        auto Work = std::make_shared<RecordingRunnable>(false);
        std::shared_ptr<Thread> Worker = Thread::Create(Work, {.Name = "Init fails"});
        REQUIRE_FALSE(Worker);
        REQUIRE(Work->Calls() == std::vector<String>{"Init"});
    }

    SECTION("A null Runnable starts no thread")
    {
        REQUIRE_FALSE(Thread::Create(std::shared_ptr<Runnable>(), {.Name = "Null"}));
    }

    SECTION("The lambda form runs one time")
    {
        std::atomic<int> Calls{0};
        std::shared_ptr<Thread> Worker = Thread::Create({.Name = "Lambda"}, [&Calls]() { ++Calls; });
        REQUIRE(Worker);
        Worker->Join();
        Worker->Join();
        REQUIRE(Calls == 1);
    }

    SECTION("A thread with settings starts")
    {
        std::shared_ptr<Thread> Worker = Thread::Create(
            {.Name = "Settings", .StackSize = 256 * 1024, .Priority = ThreadPriority::BelowNormal}, []() {});
        REQUIRE(Worker);
        Worker->Join();
    }
}

TEST_CASE("ThreadQueue with data items", "[Threading]")
{
    ThreadQueue<PlaySoundRequest> Queue;
    REQUIRE(Queue.IsEmpty());
    REQUIRE_FALSE(Queue.WaitForItems(std::chrono::milliseconds(1)));

    Queue.Push({1, 0.5f});
    Queue.Push({2, 1.0f});
    REQUIRE(Queue.WaitForItems(std::chrono::seconds(10)));

    std::vector<uint32> Ids;
    REQUIRE(Queue.Drain([&Ids](PlaySoundRequest& t_Request) { Ids.push_back(t_Request.SoundId); }) == 2);
    REQUIRE(Ids == std::vector<uint32>{1, 2});
    REQUIRE(Queue.IsEmpty());

    // Wake with no items: WaitForItems returns now, with false
    Queue.Wake();
    REQUIRE_FALSE(Queue.WaitForItems(std::chrono::seconds(10)));
}

TEST_CASE("CommandQueue from many producers", "[Threading]")
{
    constexpr int ProducerCount = 4;
    constexpr int CommandsEach = 1000;

    CommandQueue Queue;
    std::vector<std::vector<int>> Seen(ProducerCount);
    std::latch Go(1);

    std::vector<std::shared_ptr<Thread>> Producers;
    for (int Producer = 0; Producer < ProducerCount; ++Producer)
    {
        Producers.push_back(Thread::Create({.Name = "Producer " + std::to_string(Producer)},
                                           [&, Producer]()
                                           {
                                               Go.wait();
                                               for (int Index = 0; Index < CommandsEach; ++Index)
                                               {
                                                   // Only this test thread runs the commands, so Seen needs no lock
                                                   Queue.Push([&Seen, Producer, Index]()
                                                              { Seen[Producer].push_back(Index); });
                                               }
                                           }));
        REQUIRE(Producers.back());
    }

    Go.count_down();
    uint32 Executed = 0;
    while (Executed < ProducerCount * CommandsEach)
    {
        Queue.WaitForItems(std::chrono::milliseconds(10));
        Executed += Queue.Execute();
    }
    for (const std::shared_ptr<Thread>& Producer : Producers)
    {
        Producer->Join();
    }

    REQUIRE(Executed == ProducerCount * CommandsEach);
    for (const std::vector<int>& Order : Seen)
    {
        REQUIRE(Order.size() == CommandsEach);
        for (int Index = 0; Index < CommandsEach; ++Index)
        {
            REQUIRE(Order[Index] == Index);
        }
    }
}

TEST_CASE("CommandQueue runs a command pushed during Execute on the next call", "[Threading]")
{
    CommandQueue Queue;
    int Value = 0;
    Queue.Push(
        [&]()
        {
            Value = 1;
            Queue.Push([&Value]() { Value = 2; });
        });

    REQUIRE(Queue.Execute() == 1);
    REQUIRE(Value == 1);
    REQUIRE(Queue.Execute() == 1);
    REQUIRE(Value == 2);
}

TEST_CASE("CommandRunnable on a thread", "[Threading]")
{
    auto Work = std::make_shared<TickingRunnable>();
    std::shared_ptr<Thread> Worker = Thread::Create(Work, {.Name = "Commands"});
    REQUIRE(Worker);

    std::latch Ran(1);
    ThreadId RanOn = InvalidThreadId;
    Work->Enqueue(
        [&]()
        {
            RanOn = Thread::CurrentId();
            Ran.count_down();
        });
    Ran.wait();
    REQUIRE(RanOn == Worker->GetId());

    // Commands sent before Kill still run
    std::atomic<int> Late{0};
    for (int Index = 0; Index < 100; ++Index)
    {
        Work->Enqueue([&Late]() { ++Late; });
    }
    Worker->Kill();
    REQUIRE(Late == 100);
    REQUIRE(Work->Ticks > 0);
    REQUIRE(Work->IsStopRequested());
}

TEST_CASE("CommandRunnable with no thread", "[Threading]")
{
    TickingRunnable Work;
    int Value = 0;
    Work.Enqueue([&Value]() { Value = 7; });
    REQUIRE(Value == 0);
    REQUIRE(Work.ExecuteCommands() == 1);
    Work.Tick();
    REQUIRE(Value == 7);
    REQUIRE(Work.Ticks == 1);
}
