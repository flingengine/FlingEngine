#include "Input/InputDevice.h"
#include "Input/InputMessageHandler.h"

namespace Fling
{
    InputDevice::InputDevice(InputDeviceId t_Id,
                             const InputDeviceInfo& t_Info,
                             InputInterfaceId t_Owner,
                             std::weak_ptr<InputMessageHandler> t_Handler)
        : m_Id(t_Id), m_Info(t_Info), m_Owner(t_Owner), m_Handler(std::move(t_Handler))
    {
    }

    InputKeyState InputDevice::GetLatestKeyState(InputKey t_Key) const
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        return Handler ? Handler->GetLatestState().GetKeyState(m_Id, t_Key) : InputKeyState{};
    }

    void InputDevice::PushKey(InputKey t_Key, bool t_IsDown, uint32 t_Scancode, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = t_IsDown ? InputMessageType::KeyDown : InputMessageType::KeyUp;
        Message.Key = t_Key;
        Message.Scancode = t_Scancode;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
        }
    }

    void InputDevice::PushRepeat(InputKey t_Key, uint32 t_Scancode, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = InputMessageType::KeyRepeat;
        Message.Key = t_Key;
        Message.Scancode = t_Scancode;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
        }
    }

    void InputDevice::PushChar(uint32 t_CodePoint, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = InputMessageType::Char;
        Message.CodePoint = t_CodePoint;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
        }
    }

    void InputDevice::PushAxis(InputKey t_Key, float t_Value, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = InputMessageType::Axis;
        Message.Key = t_Key;
        Message.Value = t_Value;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
        }
    }

    void
    InputDevice::PushMouseMove(int32 t_DeltaX, int32 t_DeltaY, float t_PositionX, float t_PositionY, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = InputMessageType::MouseMove;
        Message.DeltaX = t_DeltaX;
        Message.DeltaY = t_DeltaY;
        Message.PositionX = t_PositionX;
        Message.PositionY = t_PositionY;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
        }
    }

    void InputDevice::PushWheel(float t_WheelX, float t_WheelY, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = InputMessageType::MouseWheel;
        Message.WheelX = t_WheelX;
        Message.WheelY = t_WheelY;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
        }
    }

    void InputDevice::PushFocus(bool t_HasFocus, int64 t_TimeNs)
    {
        if (!m_Connected)
        {
            return;
        }
        InputMessage Message;
        Message.Device = m_Id;
        Message.TimeNs = t_TimeNs;
        Message.Type = t_HasFocus ? InputMessageType::FocusGained : InputMessageType::FocusLost;
        if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
        {
            Handler->HandleMessage(Message);
            if (!t_HasFocus)
            {
                Handler->ReleaseHeldKeys(m_Id, t_TimeNs);
            }
        }
    }
} // namespace Fling
