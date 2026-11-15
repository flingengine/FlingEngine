#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputKey.h"
#include "PlatformTime.h"
#include "Strings/String.h"

#include <chrono>
#include <type_traits>

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

        /**
         * UI (for example ImGui) took or gave back the keyboard or the mouse. Uses Capture:
         * the new state. It has no device. Input::SetUICapture sends it when the state changes.
         */
        CaptureChanged,
    };

    /**
     * Which devices UI owns. InputMessageHandler stamps the state on every message, so
     * every reader (the tick and the frame side) sees the same capture at the same place
     * in the stream, and a recording replays it.
     */
    enum class InputCapture : uint8
    {
        None = 0,
        Keyboard = 1 << 0,
        Mouse = 1 << 1,
    };

    constexpr InputCapture operator|(InputCapture t_A, InputCapture t_B)
    {
        return static_cast<InputCapture>(static_cast<uint8>(t_A) | static_cast<uint8>(t_B));
    }

    constexpr bool HasCapture(InputCapture t_Value, InputCapture t_Flag)
    {
        return (static_cast<uint8>(t_Value) & static_cast<uint8>(t_Flag)) != 0;
    }

    constexpr InputCapture MakeCapture(bool t_Keyboard, bool t_Mouse)
    {
        return (t_Keyboard ? InputCapture::Keyboard : InputCapture::None) |
               (t_Mouse ? InputCapture::Mouse : InputCapture::None);
    }

    /** True if UI owns this key under t_Capture: a keyboard key while the keyboard is captured, a mouse button while
     * the mouse is. */
    inline bool IsKeyCaptured(InputCapture t_Capture, InputKey t_Key)
    {
        return (HasCapture(t_Capture, InputCapture::Keyboard) && t_Key.IsKeyboardKey()) ||
               (HasCapture(t_Capture, InputCapture::Mouse) && t_Key.IsMouseButton());
    }

    /** One input event from one device. InputMessageHandler gives it a Sequence and a time. */
    struct InputMessage
    {
        /** Increases by 1 for each message. Unique for the session. 0 means "not set yet". */
        uint64 Sequence = 0;

        /** Time in nanoseconds (PlatformTime::NowNanoseconds) when the message was received. Never decreases. */
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

        /**
         * The UI capture that applies to this message. InputMessageHandler sets it. For a
         * CaptureChanged message it is the new state, which the sender sets.
         */
        InputCapture Capture = InputCapture::None;
    };

    // The queue copies a message as 8-byte words (InputMessageQueue).
    static_assert(std::is_trivially_copyable_v<InputMessage>);

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
    String DescribeInputMessage(const InputMessage& t_Message, StringView t_DeviceName, int64 t_StartNs);

} // namespace Fling
