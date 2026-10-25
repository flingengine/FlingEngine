#include "Input/VirtualInputDeviceInterface.h"
#include "Input/InputMessageHandler.h"
#include <algorithm>
#include <string_view>

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
        for (InputMessage& Message : Messages)
        {
            if (Message.Type == InputMessageType::DeviceConnected ||
                Message.Type == InputMessageType::DeviceDisconnected)
            {
                continue;
            }
            if (std::find(m_Devices.begin(), m_Devices.end(), Message.Device) == m_Devices.end())
            {
                continue;
            }
            const std::shared_ptr<InputDevice> Device = Handler->GetDeviceFactory().Find(Message.Device);
            if (Device == nullptr || !Device->IsConnected())
            {
                continue;
            }
            if (Message.TimeNs == 0)
            {
                Message.TimeNs = t_NowNs;
            }
            Handler->HandleMessage(Message);
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
        const std::string_view Suffix = SimulatedNameSuffix;
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

    void VirtualInputDeviceInterface::Queue(const InputMessage& t_Message)
    {
        if (std::find(m_Devices.begin(), m_Devices.end(), t_Message.Device) == m_Devices.end())
        {
            return;
        }
        m_Queued.push_back(t_Message);
    }
} // namespace Fling
