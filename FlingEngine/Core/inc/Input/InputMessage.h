#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include <chrono>
#include <string>
#include <string_view>

namespace Fling
{
    /** The kind of an InputMessage. The fields that each type uses are given in InputMessage. */
    enum class InputMessageType : uint8
    {
        /** Key or button pressed. Uses Key and Scancode. */
        KeyDown,

        /** Key or button released. Uses Key and Scancode. */
        KeyUp,

        /** Operating system key repeat while a key is held. UI and text only, never gameplay. */
        KeyRepeat,

        /** Text input. Uses CodePoint. */
        Char,

        /** One sample of an analog axis. Uses Key and Value. */
        Axis,

        /** Mouse movement. Uses DeltaX, DeltaY (relative counts) and PositionX, PositionY (window position). */
        MouseMove,

        /** Mouse wheel scroll. Uses WheelX and WheelY. */
        MouseWheel,

        DeviceConnected,
        DeviceDisconnected,
        FocusLost,
        FocusGained,
    };

    /** One input event from one device. InputMessageHandler gives it a Sequence and a time. */
    struct InputMessage
    {
        /** Increases by 1 for each message. Unique for the session. 0 means "not set yet". */
        uint64 Sequence = 0;

        /** Time in nanoseconds (see InputNowNs) when the message was received. Never decreases. */
        int64 TimeNs = 0;

        InputDeviceId Device;

        InputMessageType Type = InputMessageType::KeyDown;

        InputKey Key;

        /** The platform scancode of a key, if the backend has one. 0 if not. */
        uint32 Scancode = 0;

        /** Unicode code point of a Char message. */
        uint32 CodePoint = 0;

        /** Axis value. Sticks are -1 to 1, triggers are 0 to 1. */
        float Value = 0.0f;

        /** Relative mouse movement in integer counts. Sums are exact in any order. */
        int32 DeltaX = 0;
        int32 DeltaY = 0;

        /** Cursor position in window coordinates. */
        float PositionX = 0.0f;
        float PositionY = 0.0f;

        float WheelX = 0.0f;
        float WheelY = 0.0f;

        /**
         * True if the message comes from a simulated device (playback, tests, scripts).
         * InputMessageHandler sets it from the device, so an interface cannot change it.
         */
        bool Simulated = false;
    };

    /** How much InputMessageHandler logs. Set it with --log-input or Input::SetLogLevel. */
    enum class InputLogLevel : uint8
    {
        Off,

        /** Every message except MouseMove, which can arrive at more than 1000 per second. */
        Events,

        /** Every message. */
        All,
    };

    /** The name of a message type, for example "KeyDown". */
    const char* ToString(InputMessageType t_Type);

    /**
     * A one-line description of a message for logs, for example
     * "#42 t=1.250000s Keyboard(1) KeyDown W scancode=25".
     *
     * @param t_Message     The message to describe
     * @param t_DeviceName  The display name of the message's device, or empty
     * @param t_StartNs     Times are shown in seconds after this time
     */
    std::string DescribeInputMessage(const InputMessage& t_Message, std::string_view t_DeviceName, int64 t_StartNs);

    /**
     * The current time for input messages, in nanoseconds from a steady clock.
     * Replace with PlatformTime when it exists.
     */
    inline int64 InputNowNs()
    {
        using namespace std::chrono;
        return static_cast<int64>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
    }
} // namespace Fling
