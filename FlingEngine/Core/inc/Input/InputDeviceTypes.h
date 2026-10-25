#pragma once

#include "FlingTypes.h"
#include <cstddef>
#include <functional>
#include <string>

namespace Fling
{
    /**
     * Opaque ID of one input device. It stays the same when the same device disconnects
     * and connects again. 0 is not a valid ID.
     */
    struct InputDeviceId
    {
        uint32 Value = 0;

        constexpr bool IsValid() const { return Value != 0; }

        constexpr bool operator==(const InputDeviceId& t_Other) const { return Value == t_Other.Value; }

        constexpr bool operator!=(const InputDeviceId& t_Other) const { return Value != t_Other.Value; }

        constexpr bool operator<(const InputDeviceId& t_Other) const { return Value < t_Other.Value; }
    };

    /** Opaque ID of one IInputDeviceInterface that was added to an InputMessageHandler. 0 is not a valid ID. */
    struct InputInterfaceId
    {
        uint32 Value = 0;

        constexpr bool IsValid() const { return Value != 0; }

        constexpr bool operator==(const InputInterfaceId& t_Other) const { return Value == t_Other.Value; }

        constexpr bool operator!=(const InputInterfaceId& t_Other) const { return Value != t_Other.Value; }
    };

    /** The kind of physical device. */
    enum class InputDeviceType : uint8
    {
        Unknown,
        Keyboard,
        Mouse,
        Gamepad,
        Touch,
        /** A device made by code: tests, replays, and scripts. */
        Virtual,
    };

    /** What a device can do. Use these to build UI, for example to show gamepad glyphs. */
    enum class InputDeviceCapabilities : uint32
    {
        None = 0,
        Keys = 1 << 0,
        MouseButtons = 1 << 1,
        /** An absolute cursor position in the window. */
        Pointer = 1 << 2,
        /** Relative motion, for example mouse movement. */
        RelativeMotion = 1 << 3,
        Wheel = 1 << 4,
        Buttons = 1 << 5,
        Thumbsticks = 1 << 6,
        Triggers = 1 << 7,
        Haptics = 1 << 8,
        Lighting = 1 << 9,
        Gyro = 1 << 10,
        Accelerometer = 1 << 11,
        Touchpad = 1 << 12,
    };

    constexpr InputDeviceCapabilities operator|(InputDeviceCapabilities t_A, InputDeviceCapabilities t_B)
    {
        return static_cast<InputDeviceCapabilities>(static_cast<uint32>(t_A) | static_cast<uint32>(t_B));
    }

    constexpr InputDeviceCapabilities operator&(InputDeviceCapabilities t_A, InputDeviceCapabilities t_B)
    {
        return static_cast<InputDeviceCapabilities>(static_cast<uint32>(t_A) & static_cast<uint32>(t_B));
    }

    /** Metadata about one device. The interface that finds the device fills it in. */
    struct InputDeviceInfo
    {
        InputDeviceType Type = InputDeviceType::Unknown;

        /** The name to show to a player, for example "Xbox Wireless Controller". */
        std::string DisplayName;

        /** USB HID vendor ID. 0 if not known. */
        uint16 VendorId = 0;

        /** USB HID product ID. 0 if not known. */
        uint16 ProductId = 0;

        /**
         * A string that identifies this physical device on this interface, for example a
         * GUID, a device path, or "glfw:keyboard". Used to give a device its old ID when it
         * connects again.
         */
        std::string HardwarePath;

        InputDeviceCapabilities Capabilities = InputDeviceCapabilities::None;

        bool HasCapability(InputDeviceCapabilities t_Capability) const
        {
            return (Capabilities & t_Capability) != InputDeviceCapabilities::None;
        }
    };

    /**
     * Opaque handle to a platform window. For GLFW, Native is the GLFWwindow*. Some
     * interfaces need a window to get input (mouse position, Win32 messages).
     */
    struct WindowHandle
    {
        void* Native = nullptr;

        constexpr bool IsValid() const { return Native != nullptr; }

        constexpr bool operator==(const WindowHandle& t_Other) const { return Native == t_Other.Native; }

        constexpr bool operator!=(const WindowHandle& t_Other) const { return Native != t_Other.Native; }
    };

    /** The name of a device type, for example "Gamepad". */
    const char* ToString(InputDeviceType t_Type);

    /** The names of every capability in t_Capabilities, separated by ", ", for example "Keys, Haptics". "None" if
     * empty. */
    std::string ToString(InputDeviceCapabilities t_Capabilities);
} // namespace Fling

template <>
struct std::hash<Fling::InputDeviceId>
{
    std::size_t operator()(const Fling::InputDeviceId& t_Id) const noexcept { return std::hash<uint32>{}(t_Id.Value); }
};
