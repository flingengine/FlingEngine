#include <catch2/catch_all.hpp>

#include "Input/Input.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Logger.h"
#include <vector>

namespace
{
    using namespace Fling;

    /** Starts the Input singleton, and always shuts it down at the end of the test. */
    struct InputGuard
    {
        InputGuard() { Input::Get().Init(); }

        ~InputGuard() { Input::Get().Shutdown(); }
    };

    struct DeviceListener
    {
        std::vector<uint32> Connected;
        std::vector<uint32> Disconnected;

        void OnConnected(const InputDevice& t_Device) { Connected.push_back(t_Device.GetId().Value); }

        void OnDisconnected(const InputDevice& t_Device) { Disconnected.push_back(t_Device.GetId().Value); }
    };

    InputDeviceInfo KeyboardInfo()
    {
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Keyboard;
        Info.HardwarePath = "virtual:kb";
        Info.Capabilities = InputDeviceCapabilities::Keys;
        return Info;
    }

    InputDeviceInfo MouseInfo()
    {
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Mouse;
        Info.HardwarePath = "virtual:mouse";
        Info.Capabilities = InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::Pointer |
                            InputDeviceCapabilities::RelativeMotion;
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

    InputMessage MakeMove(InputDeviceId t_Device, int32 t_DeltaX, int32 t_DeltaY, float t_X, float t_Y)
    {
        InputMessage Message;
        Message.Device = t_Device;
        Message.Type = InputMessageType::MouseMove;
        Message.DeltaX = t_DeltaX;
        Message.DeltaY = t_DeltaY;
        Message.PositionX = t_X;
        Message.PositionY = t_Y;
        return Message;
    }

    std::shared_ptr<VirtualInputDeviceInterface> AddVirtual()
    {
        const InputInterfaceId Id = Input::Get().AddInterface<VirtualInputDeviceInterface>();
        return Input::Get().FindInterface<VirtualInputDeviceInterface>(Id);
    }
} // namespace

TEST_CASE("Input singleton is safe before Init", "[Input]")
{
    Input& Singleton = Input::Get();
    REQUIRE_FALSE(Singleton.IsInitialized());
    REQUIRE_FALSE(Singleton.IsHeld(Keys::W));
    REQUIRE_FALSE(Singleton.WasPressed(Keys::W));
    REQUIRE_FALSE(Singleton.WasReleased(Keys::W));
    REQUIRE(Singleton.GetAxisValue(Keys::GamepadLeftX) == 0.0f);
    REQUIRE(Singleton.GetDevices().empty());
    REQUIRE(Singleton.GetDevice(InputDeviceId{1}) == nullptr);
    REQUIRE_FALSE(Singleton.AddInterface<VirtualInputDeviceInterface>().IsValid());

    int32 X = 5;
    int32 Y = 5;
    Singleton.GetMouseDelta(X, Y);
    REQUIRE(X == 0);
    REQUIRE(Y == 0);

    float PointerX = 0.0f;
    float PointerY = 0.0f;
    REQUIRE_FALSE(Singleton.GetPointerPosition(PointerX, PointerY));

    Singleton.Poll();
    Singleton.BeginTick();
    Singleton.EndTick();
    Singleton.Shutdown();
}

TEST_CASE("Input singleton ticks", "[Input]")
{
    InputGuard Guard;
    Input& Singleton = Input::Get();
    REQUIRE(Singleton.IsInitialized());

    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    REQUIRE(Virtual != nullptr);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());
    const InputDeviceId Mouse = Virtual->CreateDevice(MouseInfo());

    SECTION("A press shows as held and pressed, then only held")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Singleton.Poll();
        Singleton.BeginTick();
        REQUIRE(Singleton.IsHeld(Keys::W));
        REQUIRE(Singleton.WasPressed(Keys::W));
        REQUIRE_FALSE(Singleton.WasReleased(Keys::W));
        REQUIRE(Singleton.GetKeyState(Keyboard, Keys::W).IsHeld);
        Singleton.EndTick();

        Singleton.Poll();
        Singleton.BeginTick();
        REQUIRE(Singleton.IsHeld(Keys::W));
        REQUIRE_FALSE(Singleton.WasPressed(Keys::W));
        Singleton.EndTick();
    }

    SECTION("A press and release in one tick")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Virtual->Queue(MakeKey(Keyboard, Keys::W, false));
        Singleton.Poll();
        Singleton.BeginTick();
        REQUIRE(Singleton.WasPressed(Keys::W));
        REQUIRE(Singleton.WasReleased(Keys::W));
        REQUIRE_FALSE(Singleton.IsHeld(Keys::W));
        Singleton.EndTick();
    }

    SECTION("Input is not visible before BeginTick")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Singleton.Poll();
        REQUIRE_FALSE(Singleton.IsHeld(Keys::W));
        Singleton.BeginTick();
        REQUIRE(Singleton.IsHeld(Keys::W));
        Singleton.EndTick();
    }

    SECTION("Mouse delta sums and resets after EndTick")
    {
        Virtual->Queue(MakeMove(Mouse, 3, -2, 10.0f, 20.0f));
        Virtual->Queue(MakeMove(Mouse, 4, 5, 14.0f, 25.0f));
        Singleton.Poll();
        Singleton.BeginTick();

        int32 X = 0;
        int32 Y = 0;
        Singleton.GetMouseDelta(X, Y);
        REQUIRE(X == 7);
        REQUIRE(Y == 3);

        float PointerX = 0.0f;
        float PointerY = 0.0f;
        REQUIRE(Singleton.GetPointerPosition(PointerX, PointerY));
        REQUIRE(PointerX == 14.0f);
        REQUIRE(PointerY == 25.0f);
        Singleton.EndTick();

        Singleton.GetMouseDelta(X, Y);
        REQUIRE(X == 0);
        REQUIRE(Y == 0);
        REQUIRE(Singleton.GetPointerPosition(PointerX, PointerY));
        REQUIRE(PointerX == 14.0f);
    }

    SECTION("Devices are listed")
    {
        REQUIRE(Singleton.GetDevices().size() == 2);
        REQUIRE(Singleton.GetDevice(Keyboard) != nullptr);
        REQUIRE(Singleton.GetDevice(Mouse)->GetInfo().Type == InputDeviceType::Mouse);

        Virtual->DisconnectDevice(Mouse);
        REQUIRE(Singleton.GetDevices().size() == 1);
    }

    SECTION("A new processor is independent and starts with the held keys")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Singleton.Poll();

        InputMessageProcessor Other = Singleton.CreateProcessor();
        REQUIRE(Other.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);
        REQUIRE_FALSE(Other.GetState().GetKeyState(Keyboard, Keys::W).WasPressed());

        Virtual->Queue(MakeKey(Keyboard, Keys::W, false));
        Singleton.Poll();
        Singleton.BeginTick();
        Singleton.EndTick();

        REQUIRE_FALSE(Singleton.IsHeld(Keys::W));
        REQUIRE(Other.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);

        Other.ConsumeUntil();
        REQUIRE_FALSE(Other.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);
    }
}

TEST_CASE("Input singleton device events", "[Input]")
{
    InputGuard Guard;
    Input& Singleton = Input::Get();

    DeviceListener Listener;
    Singleton.OnDeviceConnected().connect<&DeviceListener::OnConnected>(Listener);
    Singleton.OnDeviceDisconnected().connect<&DeviceListener::OnDisconnected>(Listener);

    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());
    REQUIRE(Listener.Connected.empty());

    Singleton.Poll();
    REQUIRE(Listener.Connected.size() == 1);
    REQUIRE(Listener.Connected[0] == Keyboard.Value);

    Virtual->DisconnectDevice(Keyboard);
    REQUIRE(Listener.Disconnected.empty());
    Singleton.Poll();
    REQUIRE(Listener.Disconnected.size() == 1);
    REQUIRE(Listener.Disconnected[0] == Keyboard.Value);
}

TEST_CASE("UI capture gives the game neutral input", "[Input]")
{
    InputGuard Guard;
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());
    const InputDeviceId Mouse = Virtual->CreateDevice(MouseInfo());

    Input::Get().SetUICapture(true, false);
    Virtual->Queue(MakeKey(Keyboard, Keys::M, true));
    Virtual->Queue(MakeKey(Mouse, Keys::MouseRight, true));
    Virtual->Queue(MakeMove(Mouse, 4, 2, 10.0f, 10.0f));
    Input::Get().Poll();
    Input::Get().BeginTick();

    SECTION("Captured keyboard keys are not pressed or held")
    {
        REQUIRE_FALSE(Input::Get().WasPressed(Keys::M));
        REQUIRE_FALSE(Input::Get().IsHeld(Keys::M));
        REQUIRE_FALSE(Input::Get().GetKeyState(Keyboard, Keys::M).IsHeld);
    }

    SECTION("The mouse is not captured, so it still works")
    {
        REQUIRE(Input::Get().IsHeld(Keys::MouseRight));
        int32 X = 0;
        int32 Y = 0;
        Input::Get().GetMouseDelta(X, Y);
        REQUIRE(X == 4);
        REQUIRE(Y == 2);
    }

    SECTION("Mouse capture hides mouse buttons and movement")
    {
        Input::Get().SetUICapture(false, true);
        REQUIRE_FALSE(Input::Get().IsHeld(Keys::MouseRight));
        int32 X = 0;
        int32 Y = 0;
        Input::Get().GetMouseDelta(X, Y);
        REQUIRE(X == 0);
        REQUIRE(Y == 0);
        REQUIRE(Input::Get().IsHeld(Keys::M));
    }

    SECTION("Another processor still gets the captured input")
    {
        REQUIRE(Input::Get().GetGameProcessor().GetState().GetKeyState(Keyboard, Keys::M).IsHeld);
    }

    SECTION("After the capture ends, a held key is held again")
    {
        Input::Get().EndTick();
        Input::Get().SetUICapture(false, false);
        REQUIRE(Input::Get().IsHeld(Keys::M));
        REQUIRE_FALSE(Input::Get().WasPressed(Keys::M));
    }
}

TEST_CASE("Input logging does not change messages", "[Input]")
{
    Logger::Get().Init();
    InputGuard Guard;
    Input::Get().SetLogLevel(InputLogLevel::All);
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());

    Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
    Input::Get().Poll();
    Input::Get().BeginTick();
    REQUIRE(Input::Get().WasPressed(Keys::W));
    REQUIRE(Input::Get().GetHandler()->GetLogLevel() == InputLogLevel::All);

    Input::Get().SetLogLevel(InputLogLevel::Off);
    REQUIRE(Input::Get().GetHandler()->GetLogLevel() == InputLogLevel::Off);
}

TEST_CASE("Input processors are invalid after Shutdown", "[Input]")
{
    InputGuard Guard;
    Input& Singleton = Input::Get();

    InputMessageProcessor Before = Singleton.CreateProcessor();
    REQUIRE(Before.IsValid());

    Singleton.Shutdown();
    REQUIRE_FALSE(Before.IsValid());
    REQUIRE(Before.ConsumeUntil() == 0);

    Singleton.Init();
    InputMessageProcessor After = Singleton.CreateProcessor();
    REQUIRE(After.IsValid());
    REQUIRE_FALSE(Before.IsValid());
}
