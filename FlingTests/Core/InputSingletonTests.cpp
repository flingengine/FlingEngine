#include "../Common/TestScopes.h"
#include "Input/Input.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Logger.h"

#include <catch2/catch_all.hpp>
#include <vector>

namespace
{
    using namespace Fling;

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
    REQUIRE_FALSE(Singleton.IsKeyboardCaptured());
    REQUIRE(Singleton.GetDevices().empty());
    REQUIRE(Singleton.GetDevice(InputDeviceId{1}) == nullptr);
    REQUIRE_FALSE(Singleton.AddInterface<VirtualInputDeviceInterface>().IsValid());
    REQUIRE_FALSE(Singleton.CreateProcessor().IsValid());

    Singleton.Poll();
    Singleton.PollLate();
    Singleton.Shutdown();
}

TEST_CASE("Input singleton devices and processors", "[Input]")
{
    TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();
    REQUIRE(Singleton.IsInitialized());

    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    REQUIRE(Virtual != nullptr);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());
    const InputDeviceId Mouse = Virtual->CreateDevice(MouseInfo());

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
        InputMessageProcessor First = Singleton.CreateProcessor();
        REQUIRE_FALSE(First.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);

        // Another processor reading does not move this one
        REQUIRE(Other.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);

        Other.ConsumeUntil();
        REQUIRE_FALSE(Other.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);
    }
}

TEST_CASE("Input singleton device events", "[Input]")
{
    TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();

    DeviceListener Listener;
    Singleton.OnDeviceConnected().Connect<&DeviceListener::OnConnected>(Listener);
    Singleton.OnDeviceDisconnected().Connect<&DeviceListener::OnDisconnected>(Listener);

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

TEST_CASE("UI capture is stamped on the stream, and a processor still gets every message", "[Input]")
{
    TestScopes::InputScope Guard;
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());
    InputMessageProcessor Processor = Input::Get().CreateProcessor();

    Input::Get().SetUICapture(true, false);
    REQUIRE(Input::Get().IsKeyboardCaptured());
    REQUIRE_FALSE(Input::Get().IsMouseCaptured());
    Virtual->Queue(MakeKey(Keyboard, Keys::M, true));
    Input::Get().Poll();

    InputMessage Messages[8];
    const uint32 Count = Processor.Read(Messages, 8);
    REQUIRE(Count > 0);
    REQUIRE(Messages[Count - 1].Type == InputMessageType::KeyDown);
    REQUIRE(Messages[Count - 1].Capture == InputCapture::Keyboard);
    Processor.ConsumeUntil();
    REQUIRE(Processor.GetState().GetKeyState(Keyboard, Keys::M).IsHeld);
}

TEST_CASE("Input logging does not change messages", "[Input]")
{
    Logger::Get().Init();
    TestScopes::InputScope Guard;
    Input::Get().SetLogLevel(InputLogLevel::All);
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = AddVirtual();
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());

    InputMessageProcessor Processor = Input::Get().CreateProcessor();
    Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
    Input::Get().Poll();
    Processor.ConsumeUntil();
    REQUIRE(Processor.GetState().GetKeyState(Keyboard, Keys::W).WasPressed());
    REQUIRE(Input::Get().GetHandler()->GetLogLevel() == InputLogLevel::All);

    Input::Get().SetLogLevel(InputLogLevel::Off);
    REQUIRE(Input::Get().GetHandler()->GetLogLevel() == InputLogLevel::Off);
}

TEST_CASE("Input processors are invalid after Shutdown", "[Input]")
{
    TestScopes::InputScope Guard;
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
