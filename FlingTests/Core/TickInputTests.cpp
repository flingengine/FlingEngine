#include "../Common/TestScopes.h"
#include "Input/Input.h"
#include "Input/InputMessageHandler.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "PlatformTime.h"

#include <catch2/catch_all.hpp>
#include <string>

using namespace Fling;

namespace
{
    InputDeviceInfo KeyboardInfo(const char* t_Path)
    {
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Keyboard;
        Info.HardwarePath = t_Path;
        Info.Capabilities = InputDeviceCapabilities::Keys;
        return Info;
    }

    InputMessage MakeKey(InputDeviceId t_Device, InputKey t_Key, bool t_Down)
    {
        InputMessage Message;
        Message.Device = t_Device;
        Message.Type = t_Down ? InputMessageType::KeyDown : InputMessageType::KeyUp;
        Message.Key = t_Key;
        return Message;
    }

    template <typename T>
    concept HasWallClockTime = requires(const T& t_Value)
    {
        t_Value.TimeNs;
    }
    || requires(const T& t_Value)
    {
        t_Value.LastTimeNs;
    }
    || requires(const T& t_Value)
    {
        t_Value.LastChangeTimeNs;
    };
} // namespace

TEST_CASE("The input a fixed system reads has no wall clock time", "[Input][TickInput]")
{
    // InputState holds a TickInput: a fixed system cannot read a time that depends on when the tick ran
    STATIC_REQUIRE_FALSE(HasWallClockTime<InputMouseSample>);
    STATIC_REQUIRE_FALSE(HasWallClockTime<InputKeyEvent>);
    STATIC_REQUIRE_FALSE(HasWallClockTime<InputCut>);
    STATIC_REQUIRE_FALSE(HasWallClockTime<InputSlice>);
    STATIC_REQUIRE_FALSE(HasWallClockTime<TickKeyState>);
    STATIC_REQUIRE_FALSE(HasWallClockTime<TickInput>);
    // The queue's messages and the device state keep their times
    STATIC_REQUIRE(HasWallClockTime<InputMessage>);
}

TEST_CASE("TickInput builds one tick of input from the queue", "[Input][TickInput]")
{
    TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();
    const InputInterfaceId Id = Singleton.AddInterface<VirtualInputDeviceInterface>();
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = Singleton.FindInterface<VirtualInputDeviceInterface>(Id);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo("virtual:kb"));

    TickInputBuilder Builder(Singleton.GetHandler());
    TickInput Tick;

    SECTION("A press is seen by exactly one tick")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::T, true));
        Singleton.Poll();

        Builder.Build(Tick);
        REQUIRE(Tick.WasPressed(Keys::T));
        REQUIRE(Tick.IsHeld(Keys::T));

        // A second tick in the same frame: still held, but the press is not seen again
        Builder.Build(Tick);
        REQUIRE_FALSE(Tick.WasPressed(Keys::T));
        REQUIRE(Tick.IsHeld(Keys::T));
    }

    SECTION("A frame with no tick keeps its press for the next tick")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::Space, true));
        Singleton.Poll();
        // Another reader takes and clears the press, but the tick builder has its own cursor
        InputMessageProcessor Other = Singleton.CreateProcessor();
        Other.ConsumeUntil();
        Other.ClearEdges();

        Singleton.Poll();
        Builder.Build(Tick);
        REQUIRE(Tick.WasPressed(Keys::Space));
    }

    SECTION("A press and a release inside one tick are both counted")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::A, true));
        Virtual->Queue(MakeKey(Keyboard, Keys::A, false));
        Singleton.Poll();

        Builder.Build(Tick);
        REQUIRE(Tick.WasPressed(Keys::A));
        REQUIRE(Tick.WasReleased(Keys::A));
        REQUIRE_FALSE(Tick.IsHeld(Keys::A));
    }

    SECTION("Two devices are merged")
    {
        const InputDeviceId Second = Virtual->CreateDevice(KeyboardInfo("virtual:kb2"));
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Virtual->Queue(MakeKey(Second, Keys::W, true));
        Singleton.Poll();

        Builder.Build(Tick);
        REQUIRE(Tick.GetKeyState(Keys::W).PressCount == 2);
        REQUIRE(Tick.Keys.size() == 1);
    }

    SECTION("A captured keyboard gives neutral input")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Singleton.Poll();

        Builder.Build(Tick, {}, InputCapture::Keyboard);
        REQUIRE_FALSE(Tick.IsHeld(Keys::W));
        REQUIRE_FALSE(Tick.WasPressed(Keys::W));
    }
}

TEST_CASE("TickInputBuilder with no input system gives empty input", "[Input][TickInput]")
{
    TickInputBuilder Builder(nullptr);
    TickInput Tick;
    Tick.Slice.MouseSamples.push_back(InputMouseSample{1, 5, 0});
    Builder.Build(Tick);
    REQUIRE(Tick.Keys.empty());
    REQUIRE(Tick.Slice.MouseSamples.empty());
    REQUIRE(Tick.Slice.Cut == InputCut{});
}

TEST_CASE("The tick and the pending side make the same InputSlice from the same messages", "[Input][TickInput]")
{
    TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();
    const InputInterfaceId Id = Singleton.AddInterface<VirtualInputDeviceInterface>();
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = Singleton.FindInterface<VirtualInputDeviceInterface>(Id);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo("virtual:kb"));
    Singleton.Poll();

    TickInputBuilder Builder(Singleton.GetHandler());
    PendingInputReader Reader(Singleton.GetHandler());
    TickInput Tick;
    Builder.Build(Tick);
    const uint64 Start = Tick.Slice.Cut.LastSequence;
    REQUIRE(Start == Singleton.GetHandler()->GetLatestSequence());

    InputMessage Move;
    Move.Device = Keyboard;
    Move.Type = InputMessageType::MouseMove;
    Move.DeltaX = 3;
    Move.DeltaY = -2;
    Virtual->Queue(Move);
    Move.DeltaX = 4;
    Move.DeltaY = 1;
    Virtual->Queue(Move);
    Virtual->Queue(MakeKey(Keyboard, Keys::F, true));
    Singleton.Poll();

    // The pending reader sees them and consumes nothing
    InputSlice Pending;
    Reader.Read(Start, Pending);
    REQUIRE(Pending.Cut == InputCut{Start, Start + 3});
    REQUIRE(Pending.MouseSamples.size() == 2);
    REQUIRE(Pending.MouseDeltaX() == 7);
    REQUIRE(Pending.MouseDeltaY() == -1);
    REQUIRE(Pending.WasPressed(Keys::F));
    REQUIRE(Pending.KeyEvents.size() == 1);
    REQUIRE(Pending.KeyEvents[0].Sequence == Start + 3);
    Reader.Read(Start, Pending);
    REQUIRE(Pending.MouseSamples.size() == 2);

    // The tick consumes the same range and gets the same slice
    Builder.Build(Tick);
    REQUIRE(Tick.Slice.Cut == Pending.Cut);
    REQUIRE(Tick.Slice.MouseSamples == Pending.MouseSamples);
    REQUIRE(Tick.Slice.KeyEvents == Pending.KeyEvents);
    REQUIRE(Tick.Slice.MouseSamples[0] == InputMouseSample{Start + 1, 3, -2});
    REQUIRE(Tick.Slice.KeyEvents[0].Down);
    REQUIRE(Tick.Slice.MouseDeltaX() == 7);

    // One walk in stream order: the two moves, then the press
    String Order;
    ForEachInOrder(
        Tick.Slice.MouseSamples,
        Tick.Slice.KeyEvents,
        [&Order](const InputMouseSample& t_Sample) { Order += "m" + std::to_string(t_Sample.DeltaX); },
        [&Order](const InputKeyEvent& t_Key) { Order += String("k") + t_Key.Key.GetName(); });
    REQUIRE(Order == "m3m4kF");

    // After the tick's cut, nothing is pending
    Reader.Read(Tick.Slice.Cut.LastSequence, Pending);
    REQUIRE(Pending.MouseSamples.empty());
    REQUIRE(Pending.Cut == InputCut{Start + 3, Start + 3});

    // A tick that consumes nothing has an empty cut that starts where the last one ended
    Builder.Build(Tick);
    REQUIRE(Tick.Slice.Cut == InputCut{Start + 3, Start + 3});
    REQUIRE(Tick.Slice.MouseSamples.empty());

    // A captured mouse gives no samples on either side
    Virtual->Queue(Move);
    Singleton.Poll();
    Reader.Read(Tick.Slice.Cut.LastSequence, Pending, InputCapture::Mouse);
    REQUIRE(Pending.MouseSamples.empty());
    Builder.Build(Tick, {}, InputCapture::Mouse);
    REQUIRE(Tick.Slice.MouseSamples.empty());

    // A captured keyboard gives no key edges on either side
    Virtual->Queue(MakeKey(Keyboard, Keys::G, true));
    Singleton.Poll();
    Reader.Read(Tick.Slice.Cut.LastSequence, Pending, InputCapture::Keyboard);
    REQUIRE(Pending.KeyEvents.empty());
    Builder.Build(Tick, {}, InputCapture::Keyboard);
    REQUIRE(Tick.Slice.KeyEvents.empty());
}

TEST_CASE("TickInputBuilder stops at the end of the window and at a cut, in sequence order", "[time][Input]")
{
    PlatformTime::ScopedVirtualClock Clock(0);
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
    const auto Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);
    InputDeviceInfo Info;
    Info.Type = InputDeviceType::Mouse;
    Info.HardwarePath = "virtual:mouse";
    Info.Capabilities = InputDeviceCapabilities::RelativeMotion;
    const InputDeviceId Mouse = Virtual->CreateDevice(Info);
    Handler->PollInterfaces(0);
    TickInputBuilder Builder(Handler);

    // Each move's DeltaX is its time
    for (const int64 Time : {5, 9, 10, 15, 30})
    {
        InputMessage Move;
        Move.Device = Mouse;
        Move.Type = InputMessageType::MouseMove;
        Move.DeltaX = static_cast<int32>(Time);
        Move.TimeNs = Time;
        Virtual->Queue(Move);
    }
    Handler->PollInterfaces(30);
    const uint64 FirstMove = Handler->GetLatestSequence() - 4;

    TickInput Tick;
    Builder.Build(Tick, TickInputLimit{.UntilNs = 10});
    REQUIRE(Tick.Slice.MouseSamples.size() == 2);
    REQUIRE(Tick.Slice.MouseSamples[0].DeltaX == 5);
    REQUIRE(Tick.Slice.MouseSamples[1].DeltaX == 9);
    REQUIRE(Tick.Slice.Cut.LastSequence == FirstMove + 1);

    // A message at exactly the end belongs to the next window
    Builder.Build(Tick, TickInputLimit{.UntilNs = 20});
    REQUIRE(Tick.Slice.MouseSamples.size() == 2);
    REQUIRE(Tick.Slice.MouseSamples[0].DeltaX == 10);

    // A cut takes up to and including its sequence, whatever the time
    Builder.Build(Tick, TickInputLimit{.UntilSequence = FirstMove + 3});
    REQUIRE(Tick.Slice.MouseSamples.empty());
    REQUIRE(Tick.Slice.Cut.LastSequence == FirstMove + 3);
    Builder.Build(Tick, TickInputLimit{.UntilSequence = FirstMove + 4});
    REQUIRE(Tick.Slice.MouseSamples.size() == 1);
    REQUIRE(Tick.Slice.MouseSamples[0].DeltaX == 30);

    // A cut before the cursor takes nothing and keeps the cut
    Builder.Build(Tick, TickInputLimit{.UntilSequence = 1});
    REQUIRE(Tick.Slice.MouseSamples.empty());
    REQUIRE(Tick.Slice.Cut == InputCut{FirstMove + 4, FirstMove + 4});
}

TEST_CASE("After an overrun the tick and the frame side drop the same messages", "[Input][TickInput]")
{
    // Mutation note: fails if PendingInputReader shows the messages left in the queue
    // after an overrun, which the tick drops (it resyncs to the newest state).
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create(8);
    const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo("virtual:kb"));
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    const uint64 Cut = Handler->GetLatestSequence();
    TickInputBuilder Builder(Handler);

    InputMessage Move;
    Move.Device = Keyboard;
    Move.Type = InputMessageType::MouseMove;
    Move.DeltaX = 1;
    for (int i = 0; i < 20; ++i)
    {
        Virtual->Queue(Move);
    }
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());

    PendingInputReader Reader(Handler);
    InputSlice Pending;
    Reader.Read(Cut, Pending);
    REQUIRE(Pending.Overran);
    TickInput Tick;
    Builder.Build(Tick);
    REQUIRE(Tick.Slice.Overran);

    // Neither side shows a move from the lost range, and both end at the newest message
    REQUIRE(Pending.MouseSamples.empty());
    REQUIRE(Tick.Slice.MouseSamples == Pending.MouseSamples);
    REQUIRE(Pending.Cut == InputCut{Cut, Handler->GetLatestSequence()});
    REQUIRE(Tick.Slice.Cut == Pending.Cut);

    // Input after the overrun reaches both sides again, the same way
    Move.DeltaX = 5;
    Virtual->Queue(Move);
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Reader.Read(Tick.Slice.Cut.LastSequence, Pending);
    Builder.Build(Tick);
    REQUIRE_FALSE(Pending.Overran);
    REQUIRE_FALSE(Tick.Slice.Overran);
    REQUIRE(Pending.MouseSamples.size() == 1);
    REQUIRE(Tick.Slice.MouseSamples.size() == 1);
    REQUIRE(Tick.Slice.MouseSamples == Pending.MouseSamples);
}

TEST_CASE("A tick counts presses and releases from its key events only", "[Input][TickInput]")
{
    // One rule for "pressed this tick": TickInput::Keys and InputSlice::PressCount agree, also
    // after an overrun, whose resync changes the held state with no key event. Mutation note:
    // fails if MergeKeys takes the counts from the device state.
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create(8);
    const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo("virtual:kb"));
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    TickInputBuilder Builder(Handler);
    TickInput Tick;

    Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Builder.Build(Tick);
    REQUIRE(Tick.WasPressed(Keys::W));
    REQUIRE(Tick.Slice.WasPressed(Keys::W));

    // W is released in a range that the queue overwrites before the next tick
    Virtual->Queue(MakeKey(Keyboard, Keys::W, false));
    for (int i = 0; i < 20; ++i)
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::S, i % 2 == 0));
    }
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Builder.Build(Tick);
    REQUIRE(Tick.Slice.Overran);
    REQUIRE_FALSE(Tick.IsHeld(Keys::W));
    REQUIRE(Tick.GetKeyState(Keys::W).ReleaseCount == 0);
    REQUIRE(Tick.Slice.KeyEvents.empty());
    for (const auto& [Key, State] : Tick.Keys)
    {
        CAPTURE(Key.GetName());
        REQUIRE(State.PressCount == Tick.Slice.PressCount(Key));
    }
}
