#include <catch2/catch_all.hpp>

#include "Input/InputMessage.h"
#include <string>

TEST_CASE("Input message descriptions", "[Input]")
{
    using namespace Fling;

    InputMessage Message;
    Message.Sequence = 42;
    Message.TimeNs = 1'250'000'000;
    Message.Device = InputDeviceId{1};

    SECTION("Key messages give the key name and scancode")
    {
        Message.Type = InputMessageType::KeyDown;
        Message.Key = Keys::W;
        Message.Scancode = 25;
        REQUIRE(DescribeInputMessage(Message, "Keyboard", 0) == "#42 t=1.250000s Keyboard(1) KeyDown W scancode=25");
    }

    SECTION("Axis messages give the value")
    {
        Message.Type = InputMessageType::Axis;
        Message.Key = Keys::GamepadLeftX;
        Message.Value = -0.5f;
        REQUIRE_THAT(DescribeInputMessage(Message, "Pad", 0),
                     Catch::Matchers::EndsWith("Axis GamepadLeftX value=-0.500"));
    }

    SECTION("Mouse moves give the delta and position")
    {
        Message.Type = InputMessageType::MouseMove;
        Message.DeltaX = 3;
        Message.DeltaY = -2;
        Message.PositionX = 10.0f;
        Message.PositionY = 20.5f;
        REQUIRE_THAT(DescribeInputMessage(Message, "Mouse", 0),
                     Catch::Matchers::EndsWith("MouseMove delta=(3, -2) pos=(10.0, 20.5)"));
    }

    SECTION("Times are relative to the start time, and no name gives \"Device\"")
    {
        Message.Type = InputMessageType::FocusLost;
        REQUIRE(DescribeInputMessage(Message, "", 250'000'000) == "#42 t=1.000000s Device(1) FocusLost");
    }

    SECTION("A very long device name is cut, not overrun")
    {
        Message.Type = InputMessageType::DeviceConnected;
        const std::string LongName(500, 'x');
        REQUIRE(DescribeInputMessage(Message, LongName, 0).size() < 400);
    }

    SECTION("Every message type has a name")
    {
        for (int Type = 0; Type <= static_cast<int>(InputMessageType::FocusGained); ++Type)
        {
            REQUIRE(std::string(ToString(static_cast<InputMessageType>(Type))) != "Unknown");
        }
    }
}

TEST_CASE("Input device type and capability names", "[Input]")
{
    using namespace Fling;

    REQUIRE(std::string(ToString(InputDeviceType::Gamepad)) == "Gamepad");
    REQUIRE(ToString(InputDeviceCapabilities::None) == "None");
    REQUIRE(ToString(InputDeviceCapabilities::Keys | InputDeviceCapabilities::Haptics) == "Keys, Haptics");
}
