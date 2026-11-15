#include "Input/InputMessageHandler.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Threading/Lane.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <thread>

using namespace Fling;
using namespace std::chrono_literals;

// A tick on the game thread that waits for the main thread (the input writer) sleeps until
// the writer's next poll wakes it (Docs/threading-model.md section 11): it does not spin.

TEST_CASE("TickInputProvider: a tick that waits for the writer's watermark sleeps until a poll wakes it",
          "[Input][threads]")
{
    // Mutation note: a wait loop that spins (yield) checks StopWaiting millions of times in
    // the 200 ms before the poll; a wait that sleeps until a poll, or 10 ms, checks it about 20 times
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    std::atomic<uint32> Checks{0};
    std::atomic<bool> Done{false};
    TickInputProvider Provider(Handler,
                               nullptr,
                               nullptr,
                               {.CutSender = TickCutSender::Writer,
                                .TickLane = Lane::Game,
                                .WatermarkWaitNs = -1,
                                .StopWaiting = [&Checks]()
                                {
                                    Checks.fetch_add(1, std::memory_order_relaxed);
                                    return false;
                                }});
    std::thread Tick(
        [&]()
        {
            const LaneCheck::Scope GameLane(Lane::Game);
            TickInput Input;
            Provider.Build(Input, TickWindow{0, 1000, TickTrigger::Clock});
            Done.store(true, std::memory_order_release);
        });

    std::this_thread::sleep_for(200ms);
    const bool bDoneBeforePoll = Done.load(std::memory_order_acquire);
    // The writer's poll: every message before 1000 ns is in the queue
    Handler->PollInterfaces(1000);
    Tick.join();

    CHECK_FALSE(bDoneBeforePoll);
    CHECK(Done.load());
    CHECK(Checks.load() < 100);
}
