#include "Input/InputMessage.h"
#include <algorithm>
#include <cstdio>

namespace Fling
{
    namespace
    {
        /** Append the output of snprintf. A result that did not fit is cut at the end of the buffer. */
        void AppendPrinted(std::string& t_Out, const char* t_Buffer, size_t t_Size, int t_Length)
        {
            if (t_Length > 0)
            {
                t_Out.append(t_Buffer, std::min(static_cast<size_t>(t_Length), t_Size - 1));
            }
        }
    } // namespace

    const char* ToString(InputMessageType t_Type)
    {
        switch (t_Type)
        {
        case InputMessageType::KeyDown:
            return "KeyDown";
        case InputMessageType::KeyUp:
            return "KeyUp";
        case InputMessageType::KeyRepeat:
            return "KeyRepeat";
        case InputMessageType::Char:
            return "Char";
        case InputMessageType::Axis:
            return "Axis";
        case InputMessageType::MouseMove:
            return "MouseMove";
        case InputMessageType::MouseWheel:
            return "MouseWheel";
        case InputMessageType::DeviceConnected:
            return "DeviceConnected";
        case InputMessageType::DeviceDisconnected:
            return "DeviceDisconnected";
        case InputMessageType::FocusLost:
            return "FocusLost";
        case InputMessageType::FocusGained:
            return "FocusGained";
        }
        return "Unknown";
    }

    std::string DescribeInputMessage(const InputMessage& t_Message, std::string_view t_DeviceName, int64 t_StartNs)
    {
        char Buffer[192];
        const double Seconds = static_cast<double>(t_Message.TimeNs - t_StartNs) / 1e9;
        const std::string Device(t_DeviceName.empty() ? std::string_view("Device") : t_DeviceName);

        int Length = std::snprintf(Buffer,
                                   sizeof(Buffer),
                                   "#%llu t=%.6fs %s(%u) %s",
                                   static_cast<unsigned long long>(t_Message.Sequence),
                                   Seconds,
                                   Device.c_str(),
                                   static_cast<unsigned>(t_Message.Device.Value),
                                   ToString(t_Message.Type));
        std::string Result;
        AppendPrinted(Result, Buffer, sizeof(Buffer), Length);

        switch (t_Message.Type)
        {
        case InputMessageType::KeyDown:
        case InputMessageType::KeyUp:
        case InputMessageType::KeyRepeat:
            Length =
                std::snprintf(Buffer, sizeof(Buffer), " %s scancode=%u", t_Message.Key.GetName(), t_Message.Scancode);
            break;
        case InputMessageType::Char:
            Length = std::snprintf(Buffer, sizeof(Buffer), " U+%04X", t_Message.CodePoint);
            break;
        case InputMessageType::Axis:
            Length = std::snprintf(Buffer, sizeof(Buffer), " %s value=%.3f", t_Message.Key.GetName(), t_Message.Value);
            break;
        case InputMessageType::MouseMove:
            Length = std::snprintf(Buffer,
                                   sizeof(Buffer),
                                   " delta=(%d, %d) pos=(%.1f, %.1f)",
                                   t_Message.DeltaX,
                                   t_Message.DeltaY,
                                   t_Message.PositionX,
                                   t_Message.PositionY);
            break;
        case InputMessageType::MouseWheel:
            Length = std::snprintf(Buffer, sizeof(Buffer), " wheel=(%.2f, %.2f)", t_Message.WheelX, t_Message.WheelY);
            break;
        default:
            Length = 0;
            break;
        }
        AppendPrinted(Result, Buffer, sizeof(Buffer), Length);
        return Result;
    }
} // namespace Fling
