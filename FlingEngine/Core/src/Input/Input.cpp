#include "Input/Input.h"
#include <cmath>

namespace Fling
{
    void Input::Init()
    {
        m_Handler = InputMessageHandler::Create();
        m_Handler->SetLogLevel(m_LogLevel);
        m_KeyboardCaptured = false;
        m_MouseCaptured = false;
        m_GameProcessor.emplace(m_Handler->CreateProcessor());
        m_Recorder = std::make_unique<InputRecorder>(std::weak_ptr<InputMessageHandler>(m_Handler));
        m_Playback = std::make_unique<InputPlayback>(std::weak_ptr<InputMessageHandler>(m_Handler));
    }

    void Input::Shutdown()
    {
        if (m_Playback)
        {
            m_Playback->Stop();
        }
        if (m_Recorder)
        {
            m_Recorder->Stop();
        }
        m_GameProcessor.reset();
        if (m_Handler)
        {
            m_Handler->RemoveAllInterfaces();
        }
        m_Playback.reset();
        m_Recorder.reset();
        m_Handler.reset();
    }

    InputInterfaceId Input::AddInterface(std::shared_ptr<IInputDeviceInterface> t_Interface)
    {
        return m_Handler ? m_Handler->AddInterface(std::move(t_Interface)) : InputInterfaceId{};
    }

    bool Input::RemoveInterface(InputInterfaceId t_Id)
    {
        return m_Handler ? m_Handler->RemoveInterface(t_Id) : false;
    }

    std::shared_ptr<IInputDeviceInterface> Input::FindInterface(InputInterfaceId t_Id) const
    {
        return m_Handler ? m_Handler->FindInterface(t_Id) : nullptr;
    }

    std::shared_ptr<IInputDeviceInterface> Input::FindInterface(std::string_view t_Name) const
    {
        return m_Handler ? m_Handler->FindInterface(t_Name) : nullptr;
    }

    void Input::NotifyWindowCreated(WindowHandle t_Window)
    {
        if (m_Handler)
        {
            m_Handler->NotifyWindowCreated(t_Window);
        }
    }

    void Input::NotifyWindowDestroyed(WindowHandle t_Window)
    {
        if (m_Handler)
        {
            m_Handler->NotifyWindowDestroyed(t_Window);
        }
    }

    void Input::Poll(float t_DeltaSeconds)
    {
        if (!m_Handler)
        {
            return;
        }
        m_Playback->Update(t_DeltaSeconds);
        m_Handler->PollInterfaces(InputNowNs());
        m_Handler->DispatchDeferredEvents();
        m_Recorder->Update(t_DeltaSeconds);
    }

    void Input::BeginTick(int64 t_UntilNs)
    {
        if (!m_Handler || !m_GameProcessor)
        {
            return;
        }
        if (m_GameProcessor->Overran())
        {
            m_GameProcessor->Resync(m_Handler->GetLatestState(), m_Handler->GetLatestSequence());
        }
        m_GameProcessor->ConsumeUntil(t_UntilNs);
    }

    void Input::EndTick()
    {
        if (m_GameProcessor)
        {
            m_GameProcessor->ClearEdges();
        }
    }

    void Input::SetUICapture(bool t_Keyboard, bool t_Mouse)
    {
        m_KeyboardCaptured = t_Keyboard;
        m_MouseCaptured = t_Mouse;
    }

    void Input::SetLogLevel(InputLogLevel t_Level)
    {
        m_LogLevel = t_Level;
        if (m_Handler)
        {
            m_Handler->SetLogLevel(t_Level);
        }
    }

    bool Input::IsCaptured(InputKey t_Key) const
    {
        return (m_KeyboardCaptured && t_Key.IsKeyboardKey()) || (m_MouseCaptured && t_Key.IsMouseButton());
    }

    bool Input::IsHeld(InputKey t_Key) const
    {
        if (!m_GameProcessor || IsCaptured(t_Key))
        {
            return false;
        }
        for (const auto& Entry : m_GameProcessor->GetState().GetDevices())
        {
            if (Entry.second.GetKeyState(t_Key).IsHeld)
            {
                return true;
            }
        }
        return false;
    }

    bool Input::WasPressed(InputKey t_Key) const
    {
        if (!m_GameProcessor || IsCaptured(t_Key))
        {
            return false;
        }
        for (const auto& Entry : m_GameProcessor->GetState().GetDevices())
        {
            if (Entry.second.GetKeyState(t_Key).WasPressed())
            {
                return true;
            }
        }
        return false;
    }

    bool Input::WasReleased(InputKey t_Key) const
    {
        if (!m_GameProcessor || IsCaptured(t_Key))
        {
            return false;
        }
        for (const auto& Entry : m_GameProcessor->GetState().GetDevices())
        {
            if (Entry.second.GetKeyState(t_Key).WasReleased())
            {
                return true;
            }
        }
        return false;
    }

    InputKeyState Input::GetKeyState(InputDeviceId t_Device, InputKey t_Key) const
    {
        if (!m_GameProcessor || IsCaptured(t_Key))
        {
            return InputKeyState{};
        }
        return m_GameProcessor->GetState().GetKeyState(t_Device, t_Key);
    }

    float Input::GetAxisValue(InputKey t_Key) const
    {
        float Best = 0.0f;
        if (!m_GameProcessor)
        {
            return Best;
        }
        for (const auto& Entry : m_GameProcessor->GetState().GetDevices())
        {
            const float Value = Entry.second.GetKeyState(t_Key).Value;
            if (std::fabs(Value) > std::fabs(Best))
            {
                Best = Value;
            }
        }
        return Best;
    }

    void Input::GetMouseDelta(int32& t_OutX, int32& t_OutY) const
    {
        t_OutX = 0;
        t_OutY = 0;
        if (!m_GameProcessor || m_MouseCaptured)
        {
            return;
        }
        for (const auto& Entry : m_GameProcessor->GetState().GetDevices())
        {
            t_OutX += Entry.second.MouseDeltaX;
            t_OutY += Entry.second.MouseDeltaY;
        }
    }

    bool Input::GetPointerPosition(float& t_OutX, float& t_OutY) const
    {
        if (!m_Handler || !m_GameProcessor)
        {
            return false;
        }
        for (const std::shared_ptr<InputDevice>& Device : m_Handler->GetDeviceFactory().GetConnectedDevices())
        {
            if (!Device->GetInfo().HasCapability(InputDeviceCapabilities::Pointer))
            {
                continue;
            }
            const InputDeviceState* State = m_GameProcessor->GetState().Find(Device->GetId());
            if (State == nullptr)
            {
                return false;
            }
            t_OutX = State->PointerX;
            t_OutY = State->PointerY;
            return true;
        }
        return false;
    }

    std::shared_ptr<InputDevice> Input::GetDevice(InputDeviceId t_Id) const
    {
        return m_Handler ? m_Handler->GetDeviceFactory().Find(t_Id) : nullptr;
    }

    std::vector<std::shared_ptr<InputDevice>> Input::GetDevices() const
    {
        if (!m_Handler)
        {
            return {};
        }
        return m_Handler->GetDeviceFactory().GetConnectedDevices();
    }

    InputMessageProcessor Input::CreateProcessor() const
    {
        if (m_Handler)
        {
            return m_Handler->CreateProcessor();
        }
        // No handler: a processor with no queue. It reads nothing, and IsValid is false.
        return InputMessageProcessor(std::weak_ptr<const InputMessageQueue>{}, 0);
    }
} // namespace Fling
