#include "Input/InputDeviceFactory.h"
#include "Input/IInputDeviceInterface.h"
#include "Input/InputMessageHandler.h"

namespace Fling
{
    namespace
    {
        void SendConnectionMessage(InputMessageHandler& t_Handler, InputDeviceId t_Id, bool t_Connected)
        {
            InputMessage Message;
            Message.Device = t_Id;
            Message.Type = t_Connected ? InputMessageType::DeviceConnected : InputMessageType::DeviceDisconnected;
            t_Handler.HandleMessage(Message);
        }
    } // namespace

    InputDeviceFactory::InputDeviceFactory(InputMessageHandler& t_Handler) : m_Handler(t_Handler)
    {
    }

    std::shared_ptr<InputDevice> InputDeviceFactory::CreateDevice(const InputDeviceInfo& t_Info,
                                                                  const IInputDeviceInterface& t_Owner)
    {
        const std::string InterfaceName = t_Owner.GetName();

        for (auto& Entry : m_Devices)
        {
            Record& Rec = Entry.second;
            const InputDeviceInfo& Existing = Rec.Device->m_Info;
            const bool IsSame = Rec.InterfaceName == InterfaceName && Existing.Type == t_Info.Type &&
                                Existing.VendorId == t_Info.VendorId && Existing.ProductId == t_Info.ProductId &&
                                Existing.HardwarePath == t_Info.HardwarePath;
            if (!IsSame)
            {
                continue;
            }
            const std::shared_ptr<InputDevice> Device = Rec.Device;
            if (Device->m_Connected)
            {
                return Device;
            }
            Device->m_Owner = t_Owner.GetInterfaceId();
            Device->m_Info = t_Info;
            Device->m_Simulated = t_Owner.IsSimulated();
            Device->m_Connected = true;
            SendConnectionMessage(m_Handler, Device->m_Id, true);
            m_Handler.QueueDeviceEvent(Device->m_Id, true);
            return Device;
        }

        const InputDeviceId Id{m_NextId++};
        Record Rec;
        Rec.InterfaceName = InterfaceName;
        Rec.Device = std::make_shared<InputDevice>(Id, t_Info, t_Owner.GetInterfaceId(), m_Handler.weak_from_this());
        Rec.Device->m_Connected = true;
        Rec.Device->m_Simulated = t_Owner.IsSimulated();
        const std::shared_ptr<InputDevice> Device = Rec.Device;
        m_Devices.emplace(Id, std::move(Rec));
        SendConnectionMessage(m_Handler, Id, true);
        m_Handler.QueueDeviceEvent(Id, true);
        return Device;
    }

    void InputDeviceFactory::DisconnectDevice(InputDeviceId t_Id)
    {
        const std::shared_ptr<InputDevice> Device = Find(t_Id);
        if (Device == nullptr || !Device->m_Connected)
        {
            return;
        }
        m_Handler.ReleaseHeldKeys(t_Id);
        SendConnectionMessage(m_Handler, t_Id, false);
        Device->m_Connected = false;
        Device->m_Owner = InputInterfaceId{};
        m_Handler.QueueDeviceEvent(t_Id, false);
    }

    void InputDeviceFactory::DisconnectAllFrom(InputInterfaceId t_Owner)
    {
        std::vector<InputDeviceId> ToDisconnect;
        for (const auto& Entry : m_Devices)
        {
            const InputDevice& Device = *Entry.second.Device;
            if (Device.m_Connected && Device.m_Owner == t_Owner)
            {
                ToDisconnect.push_back(Entry.first);
            }
        }
        for (InputDeviceId Id : ToDisconnect)
        {
            DisconnectDevice(Id);
        }
    }

    std::shared_ptr<InputDevice> InputDeviceFactory::Find(InputDeviceId t_Id) const
    {
        auto It = m_Devices.find(t_Id);
        return It == m_Devices.end() ? nullptr : It->second.Device;
    }

    std::vector<std::shared_ptr<InputDevice>> InputDeviceFactory::GetDevices() const
    {
        std::vector<std::shared_ptr<InputDevice>> Result;
        Result.reserve(m_Devices.size());
        for (const auto& Entry : m_Devices)
        {
            Result.push_back(Entry.second.Device);
        }
        return Result;
    }

    std::vector<std::shared_ptr<InputDevice>> InputDeviceFactory::GetConnectedDevices() const
    {
        std::vector<std::shared_ptr<InputDevice>> Result;
        for (const auto& Entry : m_Devices)
        {
            if (Entry.second.Device->m_Connected)
            {
                Result.push_back(Entry.second.Device);
            }
        }
        return Result;
    }
} // namespace Fling
