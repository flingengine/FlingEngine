#include "Input/InputPlayback.h"
#include "Input/InputMessageHandler.h"
#include "Input/VirtualInputDeviceInterface.h"
#include <algorithm>
#include <cstdio>

#if WITH_INPUT_RECORDING
namespace Fling
{
    namespace
    {
        /** Allowed error when a time is compared with the playback clock. */
        /** Files store times to 1 microsecond, so a time can read back up to 0.5 us late. */
        constexpr double TimeEpsilon = 1e-6;
    } // namespace

    bool InputPlayback::Start(const std::string& t_Path, float t_StartDelay, std::string& t_OutError)
    {
        InputRecording Recording;
        if (!LoadInputRecording(t_Path, Recording, t_OutError))
        {
            m_Log.Add("Cannot play: " + t_OutError);
            return false;
        }
        Start(std::move(Recording), t_StartDelay);
        m_Path = t_Path;
        return true;
    }

    void InputPlayback::Start(InputRecording t_Recording, float t_StartDelay)
    {
        if (IsActive())
        {
            Stop();
        }

        m_Recording = std::move(t_Recording);
        m_Path.clear();
        m_Time = 0.0;
        m_NextMessage = 0;
        m_DeviceMap.clear();
        m_CountdownTotal = std::max(0.0f, t_StartDelay);
        m_CountdownRemaining = m_CountdownTotal;

        if (m_CountdownTotal > 0.0f)
        {
            m_State = InputPlaybackState::Countdown;
            char Buffer[64];
            std::snprintf(Buffer, sizeof(Buffer), "Countdown %.1f s", static_cast<double>(m_CountdownTotal));
            m_Log.Add(Buffer);
            return;
        }

        BeginPlayback();
        if (m_State == InputPlaybackState::Playing)
        {
            m_IsFirstUpdate = true;
        }
    }

    void InputPlayback::BeginPlayback()
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            m_State = InputPlaybackState::Idle;
            return;
        }

        m_Interface = std::make_shared<VirtualInputDeviceInterface>(InterfaceName);
        m_InterfaceId = Handler->AddInterface(m_Interface);
        m_DeviceMap.clear();
        for (const InputRecordingDevice& Device : m_Recording.Devices)
        {
            InputDeviceInfo Info = Device.Info;
            // Devices of one file must never share an identity.
            Info.HardwarePath = "playback:" + std::to_string(Device.Id);
            if (Info.DisplayName.empty())
            {
                Info.DisplayName = std::string("Playback ") + ToString(Info.Type);
            }
            m_DeviceMap[Device.Id] = m_Interface->CreateDevice(Info);
        }

        m_State = InputPlaybackState::Playing;
        char Buffer[64];
        std::snprintf(
            Buffer, sizeof(Buffer), " (%zu messages, %.3f s)", m_Recording.Messages.size(), m_Recording.GetLength());
        m_Log.Add("Playing " + (m_Path.empty() ? std::string("recording") : m_Path) + Buffer);
    }

    void InputPlayback::Stop()
    {
        if (m_State == InputPlaybackState::Idle)
        {
            return;
        }
        if (m_InterfaceId.IsValid())
        {
            if (const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock())
            {
                Handler->RemoveInterface(m_InterfaceId);
            }
        }
        m_Interface.reset();
        m_InterfaceId = InputInterfaceId{};
        m_DeviceMap.clear();
        m_State = InputPlaybackState::Idle;
        m_IsFirstUpdate = false;
        m_Log.Add("Stopped");
    }

    void InputPlayback::Update(float t_DeltaSeconds)
    {
        if (m_State == InputPlaybackState::Countdown)
        {
            m_CountdownRemaining -= t_DeltaSeconds;
            if (m_CountdownRemaining <= 0.0f)
            {
                m_CountdownRemaining = 0.0f;
                BeginPlayback();
                if (m_State == InputPlaybackState::Playing)
                {
                    m_Time = 0.0;
                    SendDueMessages();
                }
            }
            return;
        }
        if (m_State != InputPlaybackState::Playing)
        {
            return;
        }

        // Messages are sent by the next Poll, which runs after this Update. Stop only on
        // the Update after the last messages were queued, or they would be lost.
        if (m_NextMessage == m_Recording.Messages.size() && m_Time >= m_Recording.GetLength() - TimeEpsilon)
        {
            ++m_FinishedCount;
            m_Log.Add("Finished");
            Stop();
            return;
        }

        if (m_IsFirstUpdate)
        {
            m_IsFirstUpdate = false;
        }
        else
        {
            m_Time += static_cast<double>(t_DeltaSeconds);
        }
        SendDueMessages();
    }

    void InputPlayback::SendDueMessages()
    {
        if (!m_Interface)
        {
            return;
        }
        while (m_NextMessage < m_Recording.Messages.size() &&
               m_Recording.Messages[m_NextMessage].TimeSeconds <= m_Time + TimeEpsilon)
        {
            const InputRecordedMessage& Recorded = m_Recording.Messages[m_NextMessage];
            ++m_NextMessage;

            const auto It = m_DeviceMap.find(Recorded.Message.Device.Value);
            if (It == m_DeviceMap.end())
            {
                continue;
            }
            InputMessage Message = Recorded.Message;
            Message.Device = It->second;
            Message.Sequence = 0;
            Message.TimeNs = 0;
            m_Interface->Queue(Message);

            const InputRecordingDevice* Device = m_Recording.FindDevice(Recorded.Message.Device.Value);
            const std::string Name = (Device == nullptr || Device->Info.DisplayName.empty()) ? std::string("Playback")
                                                                                             : Device->Info.DisplayName;
            m_Log.Add(DescribeRecordedMessage(Recorded.TimeSeconds, Name, Recorded.Message));
        }
    }
} // namespace Fling
#endif // WITH_INPUT_RECORDING
