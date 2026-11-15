#include "Input/Input.h"
#include "Strings/String.h"

namespace Fling
{
    void Input::Init()
    {
        m_Handler = InputMessageHandler::Create();
        m_Handler->SetLogLevel(m_LogLevel);
        m_Handler->SetWriterLane(m_WriterLane);
#if WITH_INPUT_RECORDING
        m_Recorder = std::make_unique<InputRecorder>(std::weak_ptr<InputMessageHandler>(m_Handler));
        m_Playback = std::make_unique<InputPlayback>(std::weak_ptr<InputMessageHandler>(m_Handler));
#endif
    }

    void Input::Shutdown()
    {
#if WITH_INPUT_RECORDING
        if (m_Playback)
        {
            m_Playback->Stop();
        }
        if (m_Recorder)
        {
            m_Recorder->Stop();
        }
#endif
        if (m_Handler)
        {
            m_Handler->RemoveAllInterfaces();
        }
#if WITH_INPUT_RECORDING
        m_Playback.reset();
        m_Recorder.reset();
#endif
        m_Handler.reset();
    }

    void Input::SetWriterLane(const Lane t_Lane)
    {
        m_WriterLane = t_Lane;
        if (m_Handler)
        {
            m_Handler->SetWriterLane(t_Lane);
        }
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

    std::shared_ptr<IInputDeviceInterface> Input::FindInterface(StringView t_Name) const
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
        FLING_LANE_CHECK(m_WriterLane);
#if WITH_INPUT_RECORDING
        m_Playback->Update(t_DeltaSeconds);
#endif
        m_Handler->PollInterfaces(PlatformTime::NowNanoseconds());
        m_Handler->DispatchDeferredEvents();
#if WITH_INPUT_RECORDING
        m_Recorder->Update(t_DeltaSeconds);
#else
        (void)t_DeltaSeconds;
#endif
    }

    void Input::PollLate()
    {
        if (!m_Handler)
        {
            return;
        }
        FLING_LANE_CHECK(m_WriterLane);
#if WITH_INPUT_RECORDING
        m_Playback->UpdateLate();
#endif
        m_Handler->PollInterfaces(PlatformTime::NowNanoseconds());
        m_Handler->DispatchDeferredEvents();
    }

    void Input::SetUICapture(bool t_Keyboard, bool t_Mouse)
    {
        const InputCapture Capture = MakeCapture(t_Keyboard, t_Mouse);
        if (!m_Handler || Capture == m_Handler->GetCapture())
        {
            return;
        }
#if WITH_INPUT_RECORDING
        // A replay gets the capture of the recording, not of the live UI
        if (m_Playback->IsActive())
        {
            return;
        }
#endif
        InputMessage Message;
        Message.Type = InputMessageType::CaptureChanged;
        Message.Capture = Capture;
        m_Handler->HandleMessage(Message);
    }

    void Input::SetLogLevel(InputLogLevel t_Level)
    {
        m_LogLevel = t_Level;
        if (m_Handler)
        {
            m_Handler->SetLogLevel(t_Level);
        }
    }

    bool Input::IsKeyboardCaptured() const
    {
        ForbidInSimulation();
        return HasCapture(GetCapture(), InputCapture::Keyboard);
    }

    bool Input::IsMouseCaptured() const
    {
        ForbidInSimulation();
        return HasCapture(GetCapture(), InputCapture::Mouse);
    }

    std::shared_ptr<InputDevice> Input::GetDevice(InputDeviceId t_Id) const
    {
        ForbidInSimulation();
        return m_Handler ? m_Handler->GetDeviceFactory().Find(t_Id) : nullptr;
    }

    std::vector<std::shared_ptr<InputDevice>> Input::GetDevices() const
    {
        ForbidInSimulation();
        if (!m_Handler)
        {
            return {};
        }
        return m_Handler->GetDeviceFactory().GetConnectedDevices();
    }

    InputMessageProcessor Input::CreateProcessor() const
    {
        ForbidInSimulation();
        if (m_Handler)
        {
            return m_Handler->CreateProcessor();
        }
        // No handler: a processor with no queue. It reads nothing, and IsValid is false.
        return InputMessageProcessor(std::weak_ptr<const InputMessageQueue>{}, 0);
    }
} // namespace Fling
