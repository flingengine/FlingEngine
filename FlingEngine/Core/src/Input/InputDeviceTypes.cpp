#include "Input/InputDeviceTypes.h"

namespace Fling
{
    const char* ToString(InputDeviceType t_Type)
    {
        switch (t_Type)
        {
        case InputDeviceType::Unknown:
            return "Unknown";
        case InputDeviceType::Keyboard:
            return "Keyboard";
        case InputDeviceType::Mouse:
            return "Mouse";
        case InputDeviceType::Gamepad:
            return "Gamepad";
        case InputDeviceType::Touch:
            return "Touch";
        case InputDeviceType::Virtual:
            return "Virtual";
        }
        return "Unknown";
    }

    std::string ToString(InputDeviceCapabilities t_Capabilities)
    {
        struct Name
        {
            InputDeviceCapabilities Capability;
            const char* Text;
        };
        static constexpr Name Names[] = {
            {InputDeviceCapabilities::Keys, "Keys"},
            {InputDeviceCapabilities::MouseButtons, "MouseButtons"},
            {InputDeviceCapabilities::Pointer, "Pointer"},
            {InputDeviceCapabilities::RelativeMotion, "RelativeMotion"},
            {InputDeviceCapabilities::Wheel, "Wheel"},
            {InputDeviceCapabilities::Buttons, "Buttons"},
            {InputDeviceCapabilities::Thumbsticks, "Thumbsticks"},
            {InputDeviceCapabilities::Triggers, "Triggers"},
            {InputDeviceCapabilities::Haptics, "Haptics"},
            {InputDeviceCapabilities::Lighting, "Lighting"},
            {InputDeviceCapabilities::Gyro, "Gyro"},
            {InputDeviceCapabilities::Accelerometer, "Accelerometer"},
            {InputDeviceCapabilities::Touchpad, "Touchpad"},
        };

        std::string Result;
        for (const Name& Item : Names)
        {
            if ((t_Capabilities & Item.Capability) == InputDeviceCapabilities::None)
            {
                continue;
            }
            if (!Result.empty())
            {
                Result += ", ";
            }
            Result += Item.Text;
        }
        return Result.empty() ? "None" : Result;
    }
} // namespace Fling
