#include "Input/InputDeviceStates.h"
#include <set>

namespace Fling
{
    InputKeyState InputDeviceState::GetKeyState(InputKey t_Key) const
    {
        const auto Found = Keys.find(t_Key);
        if (Found == Keys.end())
        {
            return InputKeyState();
        }
        return Found->second;
    }

    void InputDeviceStates::Apply(const InputMessage& t_Message)
    {
        InputDeviceState& Device = m_Devices[t_Message.Device];

        switch (t_Message.Type)
        {
        case InputMessageType::KeyDown:
        {
            InputKeyState& Key = Device.Keys[t_Message.Key];
            if (!Key.IsHeld)
            {
                Key.IsHeld = true;
                ++Key.PressCount;
                Key.Value = 1.0f;
                Key.LastChangeTimeNs = t_Message.TimeNs;
            }
            break;
        }
        case InputMessageType::KeyUp:
        {
            InputKeyState& Key = Device.Keys[t_Message.Key];
            if (Key.IsHeld)
            {
                Key.IsHeld = false;
                ++Key.ReleaseCount;
                Key.Value = 0.0f;
                Key.LastChangeTimeNs = t_Message.TimeNs;
            }
            break;
        }
        case InputMessageType::KeyRepeat:
        {
            InputKeyState& Key = Device.Keys[t_Message.Key];
            ++Key.RepeatCount;
            break;
        }
        case InputMessageType::Axis:
        {
            InputKeyState& Key = Device.Keys[t_Message.Key];
            Key.Value = t_Message.Value;
            Key.LastChangeTimeNs = t_Message.TimeNs;
            break;
        }
        case InputMessageType::MouseMove:
            Device.MouseDeltaX += t_Message.DeltaX;
            Device.MouseDeltaY += t_Message.DeltaY;
            Device.PointerX = t_Message.PositionX;
            Device.PointerY = t_Message.PositionY;
            break;
        case InputMessageType::MouseWheel:
            Device.WheelX += t_Message.WheelX;
            Device.WheelY += t_Message.WheelY;
            break;
        case InputMessageType::DeviceConnected:
            Device.Connected = true;
            break;
        case InputMessageType::DeviceDisconnected:
            Device.Connected = false;
            break;
        default:
            break;
        }
    }

    void InputDeviceStates::ClearEdges()
    {
        for (auto& DevicePair : m_Devices)
        {
            InputDeviceState& Device = DevicePair.second;
            for (auto& KeyPair : Device.Keys)
            {
                KeyPair.second.ClearEdges();
            }
            Device.MouseDeltaX = 0;
            Device.MouseDeltaY = 0;
            Device.WheelX = 0.0f;
            Device.WheelY = 0.0f;
        }
    }

    const InputDeviceState* InputDeviceStates::Find(InputDeviceId t_Device) const
    {
        const auto Found = m_Devices.find(t_Device);
        return Found == m_Devices.end() ? nullptr : &Found->second;
    }

    InputKeyState InputDeviceStates::GetKeyState(InputDeviceId t_Device, InputKey t_Key) const
    {
        const InputDeviceState* Device = Find(t_Device);
        return Device ? Device->GetKeyState(t_Key) : InputKeyState();
    }

    std::vector<InputMessage> InputDeviceStates::MakeTransitionMessages(const InputDeviceStates& t_Target,
                                                                        int64 t_TimeNs) const
    {
        std::vector<InputMessage> Result;

        std::set<InputDeviceId> DeviceIds;
        for (const auto& Pair : m_Devices)
        {
            DeviceIds.insert(Pair.first);
        }
        for (const auto& Pair : t_Target.m_Devices)
        {
            DeviceIds.insert(Pair.first);
        }

        const InputDeviceState Empty;

        auto Make = [&](InputDeviceId t_Id, InputMessageType t_Type, InputKey t_Key)
        {
            InputMessage Message;
            Message.Sequence = 0;
            Message.TimeNs = t_TimeNs;
            Message.Device = t_Id;
            Message.Type = t_Type;
            Message.Key = t_Key;
            return Message;
        };

        for (const InputDeviceId Id : DeviceIds)
        {
            const InputDeviceState* FromPtr = Find(Id);
            const InputDeviceState* ToPtr = t_Target.Find(Id);
            const InputDeviceState& From = FromPtr ? *FromPtr : Empty;
            const InputDeviceState& To = ToPtr ? *ToPtr : Empty;

            if (!From.Connected && To.Connected)
            {
                Result.push_back(Make(Id, InputMessageType::DeviceConnected, InputKey()));
            }

            std::set<InputKey> KeyIds;
            for (const auto& Pair : From.Keys)
            {
                KeyIds.insert(Pair.first);
            }
            for (const auto& Pair : To.Keys)
            {
                KeyIds.insert(Pair.first);
            }

            for (const InputKey Key : KeyIds)
            {
                const InputKeyState FromKey = From.GetKeyState(Key);
                const InputKeyState ToKey = To.GetKeyState(Key);

                if (FromKey.IsHeld && !ToKey.IsHeld)
                {
                    Result.push_back(Make(Id, InputMessageType::KeyUp, Key));
                }
                else if (!FromKey.IsHeld && ToKey.IsHeld)
                {
                    Result.push_back(Make(Id, InputMessageType::KeyDown, Key));
                }

                if (Key.IsAxis() && FromKey.Value != ToKey.Value)
                {
                    InputMessage Message = Make(Id, InputMessageType::Axis, Key);
                    Message.Value = ToKey.Value;
                    Result.push_back(Message);
                }
            }

            if (From.Connected && !To.Connected)
            {
                Result.push_back(Make(Id, InputMessageType::DeviceDisconnected, InputKey()));
            }
        }

        return Result;
    }
} // namespace Fling
