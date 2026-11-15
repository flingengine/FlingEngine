#include "Input/InputMessageHandler.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Strings/String.h"

#include <algorithm>

namespace Fling
{
    void VirtualInputDeviceInterface::Init(const std::shared_ptr<InputMessageHandler>& t_Handler)
    {
        m_Handler = t_Handler;
    }

    void VirtualInputDeviceInterface::Shutdown()
    {
        m_Devices.clear();
        m_Queued.clear();
        m_QueuedTags.clear();
    }

    void VirtualInputDeviceInterface::Poll(int64 t_NowNs)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        std::vector<InputMessage> Messages;
        Messages.swap(m_Queued);
        std::vector<uint64> Tags;
        Tags.swap(m_QueuedTags);
        for (size_t Index = 0; Index < Messages.size(); ++Index)
        {
            InputMessage& Message = Messages[Index];
            if (!CanSend(*Handler, Message))
            {
                if (Tags[Index] != 0 && m_OnSent)
                {
                    m_OnSent(Tags[Index], 0);
                }
                continue;
            }
            if (Message.TimeNs == 0)
            {
                Message.TimeNs = t_NowNs;
            }
            const uint64 Sequence = Handler->HandleMessage(Message);
            if (Tags[Index] != 0 && m_OnSent)
            {
                m_OnSent(Tags[Index], Sequence);
            }
        }
    }

    InputDeviceId VirtualInputDeviceInterface::CreateDevice(const InputDeviceInfo& t_Info)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return InputDeviceId{};
        }
        // The name shows that the device is not hardware. A device from a recording of a
        // playback already has the suffix, so it is not added twice
        InputDeviceInfo Info = t_Info;
        const StringView Suffix = SimulatedNameSuffix;
        const bool HasSuffix =
            Info.DisplayName.size() >= Suffix.size() &&
            Info.DisplayName.compare(Info.DisplayName.size() - Suffix.size(), Suffix.size(), Suffix) == 0;
        if (!HasSuffix)
        {
            Info.DisplayName += Suffix;
        }
        const InputDeviceId Id = Handler->GetDeviceFactory().CreateDevice(Info, *this)->GetId();
        if (std::find(m_Devices.begin(), m_Devices.end(), Id) == m_Devices.end())
        {
            m_Devices.push_back(Id);
        }
        return Id;
    }

    void VirtualInputDeviceInterface::DisconnectDevice(InputDeviceId t_Device)
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler || std::find(m_Devices.begin(), m_Devices.end(), t_Device) == m_Devices.end())
        {
            return;
        }
        Handler->GetDeviceFactory().DisconnectDevice(t_Device);
    }

    bool VirtualInputDeviceInterface::Queue(const InputMessage& t_Message)
    {
        return Queue(t_Message, 0);
    }

    bool VirtualInputDeviceInterface::Queue(const InputMessage& t_Message, const uint64 t_Tag)
    {
        if (!IsOwnMessage(t_Message))
        {
            return false;
        }
        m_Queued.push_back(t_Message);
        m_QueuedTags.push_back(t_Tag);
        return true;
    }

    bool VirtualInputDeviceInterface::IsOwnMessage(const InputMessage& t_Message) const
    {
        if (t_Message.Type == InputMessageType::CaptureChanged)
        {
            return true;
        }
        return std::find(m_Devices.begin(), m_Devices.end(), t_Message.Device) != m_Devices.end();
    }

    bool VirtualInputDeviceInterface::CanSend(const InputMessageHandler& t_Handler, const InputMessage& t_Message) const
    {
        if (t_Message.Type == InputMessageType::DeviceConnected ||
            t_Message.Type == InputMessageType::DeviceDisconnected || !IsOwnMessage(t_Message))
        {
            return false;
        }
        if (t_Message.Type == InputMessageType::CaptureChanged)
        {
            return true;
        }
        const std::shared_ptr<InputDevice> Device = t_Handler.GetDeviceFactory().Find(t_Message.Device);
        return Device != nullptr && Device->IsConnected();
    }
} // namespace Fling
