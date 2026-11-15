// Tests of the platform clock (PlatformTime) and the tick timeline (TickTimeline).

#include "Input/InputMessageHandler.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "PlatformTime.h"
#include "TickTimeline.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

TEST_CASE("A virtual clock drives PlatformTime and input message times", "[time]")
{
    const PlatformTime::VirtualClock* Before = PlatformTime::GetVirtualClock();
    {
        PlatformTime::ScopedVirtualClock Clock(5'000);
        REQUIRE(PlatformTime::NowNanoseconds() == 5'000);
        Clock.Clock().Advance(250);
        REQUIRE(PlatformTime::NowNanoseconds() == 5'250);
        {
            PlatformTime::ScopedVirtualClock Inner(9'000);
            REQUIRE(PlatformTime::NowNanoseconds() == 9'000);
        }
        REQUIRE(PlatformTime::NowNanoseconds() == 5'250);

        // A message with no time gets the clock's time, so a virtual device is deterministic
        const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
        const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
        const auto Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Mouse;
        Info.HardwarePath = "virtual:mouse";
        Info.Capabilities = InputDeviceCapabilities::RelativeMotion;
        const InputDeviceId Mouse = Virtual->CreateDevice(Info);
        InputMessage Move;
        Move.Device = Mouse;
        Move.Type = InputMessageType::MouseMove;
        Move.DeltaX = 3;
        Virtual->Queue(Move);
        Clock.Clock().Set(7'000);
        Handler->PollInterfaces(PlatformTime::NowNanoseconds());
        InputMessage Latest;
        REQUIRE(Handler->GetQueue().TryGet(Handler->GetLatestSequence(), Latest));
        REQUIRE(Latest.TimeNs == 7'000);
    }
    REQUIRE(PlatformTime::GetVirtualClock() == Before);
}

TEST_CASE("TickTimeline: windows follow the tick number", "[time]")
{
    TickTimeline Timeline(10);
    REQUIRE_FALSE(Timeline.IsStarted());
    REQUIRE_FALSE(Timeline.IsDue(1'000'000));

    Timeline.Start(1'000);
    REQUIRE(Timeline.NextWindow() == TickWindow{1'000, 1'010, TickTrigger::Request});
    REQUIRE_FALSE(Timeline.IsDue(1'009));
    REQUIRE(Timeline.IsDue(1'010));
    REQUIRE(Timeline.Advance(TickTrigger::Clock) == TickWindow{1'000, 1'010, TickTrigger::Clock});
    REQUIRE(Timeline.NextWindow() == TickWindow{1'010, 1'020, TickTrigger::Request});

    // One day at 60 Hz: the start of each tick is its number times the step
    constexpr int64 Step = 16'666'667;
    TickTimeline Day(Step);
    Day.Start(0);
    for (uint64 i = 0; i < 60ull * 86'400ull; ++i)
    {
        Day.Advance(TickTrigger::Clock);
    }
    REQUIRE(Day.NextTick() == 60ull * 86'400ull + 1);
    REQUIRE(Day.NextWindow().StartNs == static_cast<int64>(60ull * 86'400ull) * Step);
}

TEST_CASE("TickTimeline: a new step starts at the next tick boundary, and no tick number is skipped", "[time]")
{
    TickTimeline Timeline(10);
    Timeline.Start(1'000);
    for (int i = 0; i < 3; ++i)
    {
        Timeline.Advance(TickTrigger::Clock);
    }
    REQUIRE(Timeline.NextTick() == 4);
    REQUIRE(Timeline.NextWindow() == TickWindow{1'030, 1'040, TickTrigger::Request});

    // Tick 4 starts where tick 3 ended, and is 4 long; so is every tick after it
    Timeline.SetStep(4);
    REQUIRE(Timeline.StepNs() == 4);
    REQUIRE(Timeline.NextTick() == 4);
    REQUIRE(Timeline.NextWindow() == TickWindow{1'030, 1'034, TickTrigger::Request});
    REQUIRE_FALSE(Timeline.IsDue(1'033));
    REQUIRE(Timeline.IsDue(1'034));
    REQUIRE(Timeline.Advance(TickTrigger::Clock) == TickWindow{1'030, 1'034, TickTrigger::Clock});
    REQUIRE(Timeline.Advance(TickTrigger::Clock) == TickWindow{1'034, 1'038, TickTrigger::Clock});

    // Back to a longer step; then a pause and a hitch keep the new step
    Timeline.SetStep(20);
    REQUIRE(Timeline.NextTick() == 6);
    REQUIRE(Timeline.NextWindow() == TickWindow{1'038, 1'058, TickTrigger::Request});
    Timeline.Pause(1'040);
    Timeline.Resume(1'050);
    REQUIRE(Timeline.NextWindow() == TickWindow{1'048, 1'068, TickTrigger::Request});
    REQUIRE(Timeline.DropBacklog(1'110) == 3);
    REQUIRE(Timeline.NextWindow() == TickWindow{1'108, 1'128, TickTrigger::Request});
    REQUIRE(Timeline.NextTick() == 6);

    // Before the start a new step only changes the length
    TickTimeline Unstarted(10);
    Unstarted.SetStep(5);
    Unstarted.Start(100);
    REQUIRE(Unstarted.NextWindow() == TickWindow{100, 105, TickTrigger::Request});
}

TEST_CASE("TickTimeline: a hitch drops whole ticks of time, never a tick number", "[time]")
{
    TickTimeline Timeline(10);
    Timeline.Start(0);

    // A frame at 95 may run 3 ticks: 1, 2 and 3, the windows that ended by then
    std::vector<TickWindow> Ran;
    while (Timeline.IsDue(95) && Ran.size() < 3)
    {
        Ran.push_back(Timeline.Advance(TickTrigger::Clock));
    }
    REQUIRE(Ran == std::vector<TickWindow>{
                       {0, 10, TickTrigger::Clock}, {10, 20, TickTrigger::Clock}, {20, 30, TickTrigger::Clock}});

    // Ticks 4 to 9 were due too. Their time is dropped; the next tick is 4, in the window that holds 95
    REQUIRE(Timeline.DropBacklog(95) == 6);
    REQUIRE(Timeline.NextWindow() == TickWindow{90, 100, TickTrigger::Request});
    REQUIRE_FALSE(Timeline.IsDue(95));
    REQUIRE(Timeline.DropBacklog(95) == 0);
    REQUIRE(Timeline.IsDue(100));
    REQUIRE(Timeline.NextTick() == 4);
    Timeline.Advance(TickTrigger::Clock);
    REQUIRE(Timeline.NextTick() == 5);
    REQUIRE(Timeline.NextWindow() == TickWindow{100, 110, TickTrigger::Request});
}

TEST_CASE("TickTimeline: a pause moves the windows of the ticks that have not run", "[time]")
{
    TickTimeline Timeline(10);
    Timeline.Start(0);
    REQUIRE(Timeline.Advance(TickTrigger::Clock) == TickWindow{0, 10, TickTrigger::Clock});

    Timeline.Pause(15);
    REQUIRE(Timeline.IsPaused());
    REQUIRE_FALSE(Timeline.IsDue(1'000));

    // Paused for 90: tick 2 was [10, 20), now [100, 110). Its number does not change.
    Timeline.Resume(105);
    REQUIRE_FALSE(Timeline.IsPaused());
    REQUIRE(Timeline.NextWindow() == TickWindow{100, 110, TickTrigger::Request});
    REQUIRE_FALSE(Timeline.IsDue(109));
    REQUIRE(Timeline.IsDue(110));

    // Pause and resume at once moves nothing; a second Pause keeps the first time
    Timeline.Pause(200);
    Timeline.Pause(250);
    Timeline.Resume(260);
    REQUIRE(Timeline.NextWindow() == TickWindow{160, 170, TickTrigger::Request});
}

TEST_CASE("TickTimeline: a pause before the start, and a backlog that ends on a boundary", "[time]")
{
    // A pause and resume before Start moves nothing: Start says where the first window is
    TickTimeline Timeline(10);
    Timeline.Pause(5);
    Timeline.Resume(50);
    Timeline.Start(100);
    REQUIRE(Timeline.NextWindow() == TickWindow{100, 110, TickTrigger::Request});

    // At exactly the end of the next window, that tick is due: dropping the backlog drops
    // its length of time, and the next window starts at now
    REQUIRE(Timeline.IsDue(110));
    REQUIRE(Timeline.DropBacklog(110) == 1);
    REQUIRE(Timeline.NextTick() == 1);
    REQUIRE(Timeline.NextWindow() == TickWindow{110, 120, TickTrigger::Request});
}
