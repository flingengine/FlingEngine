#include "Input/Input.h"
#include "Input/TickInput.h"
#include "Input/VirtualInputDeviceInterface.h"

#include <catch2/catch_all.hpp>

using namespace Fling;

namespace
{
    struct InputGuard
    {
        InputGuard() { Input::Get().Init(); }

        ~InputGuard() { Input::Get().Shutdown(); }
    };

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
} // namespace

TEST_CASE("TickInput builds one tick of input from the queue", "[Input][TickInput]")
{
    InputGuard Guard;
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
        // The per-frame game processor reads and clears the press, but the tick builder has its own cursor
        Singleton.BeginTick();
        Singleton.EndTick();

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

        Builder.Build(Tick, true, false);
        REQUIRE_FALSE(Tick.IsHeld(Keys::W));
        REQUIRE_FALSE(Tick.WasPressed(Keys::W));
    }
}

TEST_CASE("TickInputBuilder with no input system gives empty input", "[Input][TickInput]")
{
    TickInputBuilder Builder(nullptr);
    TickInput Tick;
    Tick.MouseDeltaX = 5;
    Builder.Build(Tick);
    REQUIRE(Tick.Keys.empty());
    REQUIRE(Tick.MouseDeltaX == 0);
}
