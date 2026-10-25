#include "Input/InputMessageHandler.h"
#include "Logger.h"
#include <algorithm>
#include <cstdio>
#include <string>

namespace Fling
{
    std::shared_ptr<InputMessageHandler> InputMessageHandler::Create(uint32 t_QueueCapacity)
    {
        return std::make_shared<InputMessageHandler>(CreateKey{}, t_QueueCapacity);
    }

    InputMessageHandler::InputMessageHandler(CreateKey, uint32 t_QueueCapacity)
        : m_Factory(*this), m_Queue(std::make_shared<InputMessageQueue>(t_QueueCapacity))
    {
    }

    InputMessageHandler::~InputMessageHandler()
    {
        RemoveAllInterfaces();
    }

    InputInterfaceId InputMessageHandler::AddInterface(std::shared_ptr<IInputDeviceInterface> t_Interface)
    {
        if (!t_Interface)
        {
            return InputInterfaceId{};
        }
        InterfaceEntry Entry;
        Entry.Id = InputInterfaceId{m_NextInterfaceId++};
        t_Interface->m_InterfaceId = Entry.Id;
        Entry.Interface = std::move(t_Interface);
        const InputInterfaceId Id = Entry.Id;

        if (m_IsPolling)
        {
            m_PendingAdds.push_back(std::move(Entry));
        }
        else
        {
            AddInterfaceNow(std::move(Entry));
        }
        return Id;
    }

    void InputMessageHandler::AddInterfaceNow(InterfaceEntry t_Entry)
    {
        // Keep a reference, so the interface stays alive if Init removes it.
        const std::shared_ptr<IInputDeviceInterface> Interface = t_Entry.Interface;
        m_Interfaces.push_back(std::move(t_Entry));
        Interface->Init(shared_from_this());
        if (m_Window.IsValid())
        {
            Interface->OnWindowCreated(m_Window);
        }
    }

    bool InputMessageHandler::RemoveInterface(InputInterfaceId t_Id)
    {
        if (FindInterface(t_Id) == nullptr)
        {
            return false;
        }
        if (m_IsPolling)
        {
            auto Pending = std::find_if(m_PendingAdds.begin(),
                                        m_PendingAdds.end(),
                                        [&](const InterfaceEntry& t_Entry) { return t_Entry.Id == t_Id; });
            if (Pending != m_PendingAdds.end())
            {
                m_PendingAdds.erase(Pending);
                return true;
            }
            if (std::find(m_PendingRemoves.begin(), m_PendingRemoves.end(), t_Id) == m_PendingRemoves.end())
            {
                m_PendingRemoves.push_back(t_Id);
            }
            return true;
        }
        RemoveInterfaceNow(t_Id);
        return true;
    }

    void InputMessageHandler::RemoveInterfaceNow(InputInterfaceId t_Id)
    {
        auto It = std::find_if(m_Interfaces.begin(),
                               m_Interfaces.end(),
                               [&](const InterfaceEntry& t_Entry) { return t_Entry.Id == t_Id; });
        if (It == m_Interfaces.end())
        {
            return;
        }
        // Keep a reference, so the interface stays alive until this function is done.
        const std::shared_ptr<IInputDeviceInterface> Interface = It->Interface;
        Interface->Shutdown();
        m_Factory.DisconnectAllFrom(t_Id);
        // Shutdown or a disconnect may have changed the list, so find the entry again.
        It = std::find_if(m_Interfaces.begin(),
                          m_Interfaces.end(),
                          [&](const InterfaceEntry& t_Entry) { return t_Entry.Id == t_Id; });
        if (It != m_Interfaces.end())
        {
            m_Interfaces.erase(It);
        }
    }

    void InputMessageHandler::RemoveAllInterfaces()
    {
        m_PendingAdds.clear();
        m_PendingRemoves.clear();
        while (!m_Interfaces.empty())
        {
            RemoveInterfaceNow(m_Interfaces.back().Id);
        }
    }

    std::shared_ptr<IInputDeviceInterface> InputMessageHandler::FindInterface(InputInterfaceId t_Id) const
    {
        for (const InterfaceEntry& Entry : m_Interfaces)
        {
            if (Entry.Id == t_Id)
            {
                return Entry.Interface;
            }
        }
        for (const InterfaceEntry& Entry : m_PendingAdds)
        {
            if (Entry.Id == t_Id)
            {
                return Entry.Interface;
            }
        }
        return nullptr;
    }

    std::shared_ptr<IInputDeviceInterface> InputMessageHandler::FindInterface(std::string_view t_Name) const
    {
        for (const InterfaceEntry& Entry : m_Interfaces)
        {
            if (t_Name == Entry.Interface->GetName())
            {
                return Entry.Interface;
            }
        }
        for (const InterfaceEntry& Entry : m_PendingAdds)
        {
            if (t_Name == Entry.Interface->GetName())
            {
                return Entry.Interface;
            }
        }
        return nullptr;
    }

    std::vector<std::shared_ptr<IInputDeviceInterface>> InputMessageHandler::GetInterfaces() const
    {
        std::vector<std::shared_ptr<IInputDeviceInterface>> Result;
        Result.reserve(m_Interfaces.size());
        for (const InterfaceEntry& Entry : m_Interfaces)
        {
            Result.push_back(Entry.Interface);
        }
        return Result;
    }

    void InputMessageHandler::PollInterfaces(int64 t_NowNs)
    {
        m_IsPolling = true;
        // Interfaces cannot be added or removed while polling, so the indices stay valid.
        for (size_t Index = 0; Index < m_Interfaces.size(); ++Index)
        {
            const InputInterfaceId Id = m_Interfaces[Index].Id;
            if (std::find(m_PendingRemoves.begin(), m_PendingRemoves.end(), Id) != m_PendingRemoves.end())
            {
                continue;
            }
            const std::shared_ptr<IInputDeviceInterface> Interface = m_Interfaces[Index].Interface;
            if (Interface->WantsPoll(t_NowNs))
            {
                Interface->Poll(t_NowNs);
            }
        }
        m_IsPolling = false;

        std::vector<InputInterfaceId> Removes;
        Removes.swap(m_PendingRemoves);
        for (InputInterfaceId Id : Removes)
        {
            RemoveInterfaceNow(Id);
        }

        std::vector<InterfaceEntry> Adds;
        Adds.swap(m_PendingAdds);
        for (InterfaceEntry& Entry : Adds)
        {
            AddInterfaceNow(std::move(Entry));
        }
    }

    void InputMessageHandler::QueueDeviceEvent(InputDeviceId t_Device, bool t_Connected)
    {
        DeferredDeviceEvent Event;
        Event.Device = t_Device;
        Event.Connected = t_Connected;
        m_PendingDeviceEvents.push_back(Event);
    }

    void InputMessageHandler::DispatchDeferredEvents()
    {
        std::vector<DeferredDeviceEvent> Events;
        Events.swap(m_PendingDeviceEvents);
        for (const DeferredDeviceEvent& Event : Events)
        {
            const std::shared_ptr<InputDevice> Device = m_Factory.Find(Event.Device);
            if (Device == nullptr)
            {
                continue;
            }
            if (Event.Connected)
            {
                m_OnDeviceConnected.publish(*Device);
            }
            else
            {
                m_OnDeviceDisconnected.publish(*Device);
            }
        }
    }

    void InputMessageHandler::HandleMessage(InputMessage t_Message)
    {
        if (t_Message.TimeNs == 0)
        {
            t_Message.TimeNs = InputNowNs();
        }
        t_Message.TimeNs = std::max(t_Message.TimeNs, m_LastTimeNs);
        m_LastTimeNs = t_Message.TimeNs;
        t_Message.Sequence = ++m_LastSequence;
        m_LatestState.Apply(t_Message);
        m_Queue->Push(t_Message);

        // Device connects and disconnects are always logged, at every log level.
        const bool IsDeviceChange = t_Message.Type == InputMessageType::DeviceConnected ||
                                    t_Message.Type == InputMessageType::DeviceDisconnected;
        if (IsDeviceChange || m_LogLevel == InputLogLevel::All ||
            (m_LogLevel == InputLogLevel::Events && t_Message.Type != InputMessageType::MouseMove))
        {
            LogMessage(t_Message);
        }
    }

    void InputMessageHandler::LogMessage(const InputMessage& t_Message) const
    {
        // Input can start before the logger (for example in tests). Then there is nothing to log to.
        if (!Logger::GetCurrentConsole() || !Logger::GetCurrentLogFile())
        {
            return;
        }

        const std::shared_ptr<InputDevice> Device = m_Factory.Find(t_Message.Device);
        const std::string_view Name = Device != nullptr ? std::string_view(Device->GetInfo().DisplayName) : "";
        std::string Line = DescribeInputMessage(t_Message, Name, m_StartNs);

        // A connect or disconnect also gives what the device is, to help find device problems
        if (Device != nullptr && (t_Message.Type == InputMessageType::DeviceConnected ||
                                  t_Message.Type == InputMessageType::DeviceDisconnected))
        {
            const InputDeviceInfo& Info = Device->GetInfo();
            char Ids[16];
            std::snprintf(Ids, sizeof(Ids), "%04X:%04X", Info.VendorId, Info.ProductId);
            Line += std::string(" (") + ToString(Info.Type) + ", " + Ids + ", " +
                    (Info.HardwarePath.empty() ? "no hardware path" : Info.HardwarePath) + ")";
        }
        F_LOG_TRACE("[Input] {}", Line);
    }

    void InputMessageHandler::ReleaseHeldKeys(InputDeviceId t_Device, int64 t_TimeNs)
    {
        const InputDeviceState* State = m_LatestState.Find(t_Device);
        if (State == nullptr)
        {
            return;
        }

        struct Release
        {
            InputKey Key;
            bool IsAxisOnly = false;
        };
        std::vector<Release> Releases;
        for (const auto& Entry : State->Keys)
        {
            if (Entry.second.IsHeld)
            {
                Releases.push_back({Entry.first, false});
            }
            else if (Entry.first.IsAxis() && Entry.second.Value != 0.0f)
            {
                Releases.push_back({Entry.first, true});
            }
        }
        std::sort(
            Releases.begin(), Releases.end(), [](const Release& t_A, const Release& t_B) { return t_A.Key < t_B.Key; });

        for (const Release& Item : Releases)
        {
            InputMessage Message;
            Message.Device = t_Device;
            Message.TimeNs = t_TimeNs;
            Message.Key = Item.Key;
            if (Item.IsAxisOnly)
            {
                Message.Type = InputMessageType::Axis;
                Message.Value = 0.0f;
            }
            else
            {
                Message.Type = InputMessageType::KeyUp;
            }
            HandleMessage(Message);
        }
    }

    void InputMessageHandler::NotifyWindowCreated(WindowHandle t_Window)
    {
        m_Window = t_Window;
        // Copy the list, so an interface can add or remove interfaces from this call.
        for (const std::shared_ptr<IInputDeviceInterface>& Interface : GetInterfaces())
        {
            // Skip an interface that an earlier call removed.
            if (FindInterface(Interface->GetInterfaceId()) == Interface)
            {
                Interface->OnWindowCreated(t_Window);
            }
        }
    }

    void InputMessageHandler::NotifyWindowDestroyed(WindowHandle t_Window)
    {
        for (const std::shared_ptr<IInputDeviceInterface>& Interface : GetInterfaces())
        {
            // Skip an interface that an earlier call removed.
            if (FindInterface(Interface->GetInterfaceId()) == Interface)
            {
                Interface->OnWindowDestroyed(t_Window);
            }
        }
        if (m_Window == t_Window)
        {
            m_Window = WindowHandle{};
        }
    }

    InputMessageProcessor InputMessageHandler::CreateProcessor() const
    {
        InputMessageProcessor Processor(m_Queue, m_LastSequence);
        Processor.Resync(m_LatestState, m_LastSequence);
        Processor.ClearEdges();
        return Processor;
    }
} // namespace Fling
