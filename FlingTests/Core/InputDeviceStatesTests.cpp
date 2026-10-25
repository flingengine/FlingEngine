#include <catch2/catch_all.hpp>

#include "Input/InputDeviceStates.h"

namespace
{
    using namespace Fling;

    const InputDeviceId DeviceA{1};
    const InputDeviceId DeviceB{2};

    /** Make a message with the fields that most tests need. */
    InputMessage Msg(uint64 t_Seq, InputDeviceId t_Device, InputMessageType t_Type, InputKey t_Key = Keys::Invalid)
    {
        InputMessage Message;
        Message.Sequence = t_Seq;
        Message.TimeNs = static_cast<int64>(t_Seq) * 1000;
        Message.Device = t_Device;
        Message.Type = t_Type;
        Message.Key = t_Key;
        return Message;
    }
} // namespace

TEST_CASE("Input device states keys", "[Input]")
{
    InputDeviceStates States;

    SECTION("Press and release before a clear keep both edges")
    {
        States.Apply(Msg(1, DeviceA, InputMessageType::KeyDown, Keys::W));
        States.Apply(Msg(2, DeviceA, InputMessageType::KeyUp, Keys::W));

        const InputKeyState Key = States.GetKeyState(DeviceA, Keys::W);
        REQUIRE(Key.PressCount == 1);
        REQUIRE(Key.ReleaseCount == 1);
        REQUIRE_FALSE(Key.IsHeld);
        REQUIRE(Key.WasPressed());
        REQUIRE(Key.WasReleased());
        REQUIRE(Key.LastChangeTimeNs == 2000);
    }

    SECTION("A second KeyDown gives one press")
    {
        States.Apply(Msg(1, DeviceA, InputMessageType::KeyDown, Keys::W));
        States.Apply(Msg(2, DeviceA, InputMessageType::KeyDown, Keys::W));

        const InputKeyState Key = States.GetKeyState(DeviceA, Keys::W);
        REQUIRE(Key.PressCount == 1);
        REQUIRE(Key.IsHeld);
        REQUIRE(Key.Value == 1.0f);
    }

    SECTION("Repeats are not presses")
    {
        States.Apply(Msg(1, DeviceA, InputMessageType::KeyDown, Keys::W));
        States.Apply(Msg(2, DeviceA, InputMessageType::KeyRepeat, Keys::W));
        States.Apply(Msg(3, DeviceA, InputMessageType::KeyRepeat, Keys::W));

        const InputKeyState Key = States.GetKeyState(DeviceA, Keys::W);
        REQUIRE(Key.PressCount == 1);
        REQUIRE(Key.RepeatCount == 2);
    }

    SECTION("ClearEdges keeps held state and value")
    {
        States.Apply(Msg(1, DeviceA, InputMessageType::KeyDown, Keys::W));
        InputMessage Axis = Msg(2, DeviceA, InputMessageType::Axis, Keys::GamepadLeftX);
        Axis.Value = 0.5f;
        States.Apply(Axis);
        States.ClearEdges();

        const InputKeyState Key = States.GetKeyState(DeviceA, Keys::W);
        REQUIRE(Key.IsHeld);
        REQUIRE(Key.Value == 1.0f);
        REQUIRE(Key.PressCount == 0);
        REQUIRE(States.GetKeyState(DeviceA, Keys::GamepadLeftX).Value == 0.5f);
    }

    SECTION("Two devices keep separate state")
    {
        States.Apply(Msg(1, DeviceA, InputMessageType::KeyDown, Keys::W));
        States.Apply(Msg(2, DeviceB, InputMessageType::KeyUp, Keys::W));

        REQUIRE(States.GetKeyState(DeviceA, Keys::W).IsHeld);
        REQUIRE_FALSE(States.GetKeyState(DeviceB, Keys::W).IsHeld);
        REQUIRE(States.GetDevices().size() == 2);
    }

    SECTION("Unknown device or key gives a default state")
    {
        REQUIRE(States.Find(DeviceA) == nullptr);
        REQUIRE_FALSE(States.GetKeyState(DeviceA, Keys::W).IsHeld);

        States.Apply(Msg(1, DeviceA, InputMessageType::KeyDown, Keys::W));
        REQUIRE(States.Find(DeviceA) != nullptr);
        const InputKeyState Other = States.GetKeyState(DeviceA, Keys::S);
        REQUIRE_FALSE(Other.IsHeld);
        REQUIRE(Other.PressCount == 0);
    }

    SECTION("Connect messages set the connected state")
    {
        States.Apply(Msg(1, DeviceA, InputMessageType::DeviceConnected));
        REQUIRE(States.Find(DeviceA)->Connected);
        States.Apply(Msg(2, DeviceA, InputMessageType::DeviceDisconnected));
        REQUIRE_FALSE(States.Find(DeviceA)->Connected);
    }
}

TEST_CASE("Input device states mouse", "[Input]")
{
    InputDeviceStates States;

    SECTION("Deltas sum exactly and the pointer is the last position")
    {
        InputMessage First = Msg(1, DeviceA, InputMessageType::MouseMove);
        First.DeltaX = 3;
        First.DeltaY = -2;
        First.PositionX = 10.0f;
        First.PositionY = 20.0f;
        InputMessage Second = Msg(2, DeviceA, InputMessageType::MouseMove);
        Second.DeltaX = 4;
        Second.DeltaY = 5;
        Second.PositionX = 14.0f;
        Second.PositionY = 25.0f;
        States.Apply(First);
        States.Apply(Second);

        const InputDeviceState* Device = States.Find(DeviceA);
        REQUIRE(Device->MouseDeltaX == 7);
        REQUIRE(Device->MouseDeltaY == 3);
        REQUIRE(Device->PointerX == 14.0f);
        REQUIRE(Device->PointerY == 25.0f);

        States.ClearEdges();
        Device = States.Find(DeviceA);
        REQUIRE(Device->MouseDeltaX == 0);
        REQUIRE(Device->MouseDeltaY == 0);
        REQUIRE(Device->PointerX == 14.0f);
    }

    SECTION("Wheel sums")
    {
        InputMessage First = Msg(1, DeviceA, InputMessageType::MouseWheel);
        First.WheelY = 1.0f;
        InputMessage Second = Msg(2, DeviceA, InputMessageType::MouseWheel);
        Second.WheelX = 2.0f;
        Second.WheelY = 3.0f;
        States.Apply(First);
        States.Apply(Second);

        const InputDeviceState* Device = States.Find(DeviceA);
        REQUIRE(Device->WheelX == 2.0f);
        REQUIRE(Device->WheelY == 4.0f);
    }
}

TEST_CASE("Input device states transition messages", "[Input]")
{
    InputDeviceStates From;
    InputDeviceStates To;

    From.Apply(Msg(1, DeviceA, InputMessageType::DeviceConnected));
    From.Apply(Msg(2, DeviceA, InputMessageType::KeyDown, Keys::W));
    From.Apply(Msg(3, DeviceB, InputMessageType::DeviceConnected));

    To.Apply(Msg(1, DeviceA, InputMessageType::DeviceConnected));
    To.Apply(Msg(2, DeviceA, InputMessageType::KeyDown, Keys::S));
    InputMessage Axis = Msg(3, DeviceA, InputMessageType::Axis, Keys::GamepadLeftX);
    Axis.Value = 0.25f;
    To.Apply(Axis);
    To.Apply(Msg(4, DeviceB, InputMessageType::DeviceDisconnected));

    const std::vector<InputMessage> Messages = From.MakeTransitionMessages(To, 777);

    SECTION("The messages are right and in order")
    {
        REQUIRE(Messages.size() == 4);
        for (const InputMessage& Message : Messages)
        {
            REQUIRE(Message.Sequence == 0);
            REQUIRE(Message.TimeNs == 777);
        }

        // Device A: keys in key id order (W before S in the list is not assumed).
        bool SawKeyUpW = false;
        bool SawKeyDownS = false;
        bool SawAxis = false;
        for (std::size_t i = 0; i < 3; ++i)
        {
            REQUIRE(Messages[i].Device == DeviceA);
            if (Messages[i].Type == InputMessageType::KeyUp && Messages[i].Key == Keys::W)
            {
                SawKeyUpW = true;
            }
            else if (Messages[i].Type == InputMessageType::KeyDown && Messages[i].Key == Keys::S)
            {
                SawKeyDownS = true;
            }
            else if (Messages[i].Type == InputMessageType::Axis && Messages[i].Key == Keys::GamepadLeftX)
            {
                SawAxis = true;
                REQUIRE(Messages[i].Value == 0.25f);
            }
        }
        REQUIRE(SawKeyUpW);
        REQUIRE(SawKeyDownS);
        REQUIRE(SawAxis);

        REQUIRE(Messages[3].Device == DeviceB);
        REQUIRE(Messages[3].Type == InputMessageType::DeviceDisconnected);
    }

    SECTION("Applying them makes the state match the target")
    {
        for (const InputMessage& Message : Messages)
        {
            From.Apply(Message);
        }

        REQUIRE_FALSE(From.GetKeyState(DeviceA, Keys::W).IsHeld);
        REQUIRE(From.GetKeyState(DeviceA, Keys::S).IsHeld);
        REQUIRE(From.GetKeyState(DeviceA, Keys::GamepadLeftX).Value == 0.25f);
        REQUIRE(From.Find(DeviceA)->Connected);
        REQUIRE_FALSE(From.Find(DeviceB)->Connected);
        REQUIRE(From.MakeTransitionMessages(To, 0).empty());
    }

    SECTION("A device only in the target is connected first")
    {
        InputDeviceStates Empty;
        const std::vector<InputMessage> Result = Empty.MakeTransitionMessages(To, 1);
        REQUIRE_FALSE(Result.empty());
        REQUIRE(Result.front().Type == InputMessageType::DeviceConnected);
        REQUIRE(Result.front().Device == DeviceA);
    }
}
