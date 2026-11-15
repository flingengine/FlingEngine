#include "Input/InputMessageHandler.h"
#include "Input/InputRecorder.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Logger.h"
#include "Strings/String.h"

#include <algorithm>
#include <cstdio>

#if WITH_INPUT_RECORDING
namespace Fling
{
    void InputRecorder::Start(const StringView t_Path, float t_StartDelay, float t_Duration)
    {
        if (IsActive())
        {
            Stop();
        }

        m_Path = t_Path;
        m_Recording = InputRecording{};
        m_LastError.clear();
        m_Elapsed = 0.0;
        m_Duration = std::max(0.0f, t_Duration);
        m_CountdownTotal = std::max(0.0f, t_StartDelay);
        m_CountdownRemaining = m_CountdownTotal;
        m_Processor.reset();

        if (m_CountdownTotal > 0.0f)
        {
            m_State = InputRecorderState::Countdown;
            char Buffer[64];
            std::snprintf(Buffer, sizeof(Buffer), "Countdown %.1f s", static_cast<double>(m_CountdownTotal));
            m_Log.Add(Buffer);
        }
        else
        {
            BeginRecording();
        }
    }

    bool InputRecorder::SetTickRate(const uint32 t_Hz)
    {
        if (t_Hz == m_TickRate)
        {
            return true;
        }
        if (IsActive())
        {
            m_Log.Add("Tick rate change to " + std::to_string(t_Hz) + " Hz refused: a recording stores one rate");
            F_LOG_WARN("Input recorder: tick rate change to {} Hz refused while it records at {} Hz", t_Hz, m_TickRate);
            return false;
        }
        m_TickRate = t_Hz;
        return true;
    }

    bool InputRecorder::Stop(bool t_TrimStopClick)
    {
        switch (m_State)
        {
        case InputRecorderState::Idle:
            return true;
        case InputRecorderState::Countdown:
            m_State = InputRecorderState::Idle;
            m_Log.Add("Cancelled");
            return true;
        case InputRecorderState::Recording:
            ReadMessages();
            if (t_TrimStopClick)
            {
                TrimStopClick();
            }
            return FinishRecording();
        }
        return true;
    }

    void InputRecorder::Update(float t_DeltaSeconds)
    {
        if (m_State == InputRecorderState::Countdown)
        {
            m_CountdownRemaining -= t_DeltaSeconds;
            if (m_CountdownRemaining <= 0.0f)
            {
                m_CountdownRemaining = 0.0f;
                BeginRecording();
            }
        }
        else if (m_State == InputRecorderState::Recording)
        {
            ReadMessages();
            TakeTickCuts();
            m_Elapsed += static_cast<double>(t_DeltaSeconds);
            if (m_Duration > 0.0f && m_Elapsed >= static_cast<double>(m_Duration))
            {
                FinishRecording();
            }
        }
    }

    void InputRecorder::BeginRecording()
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            m_State = InputRecorderState::Idle;
            return;
        }

        m_Processor.emplace(Handler->CreateProcessor());
        // Start after the newest message, so nothing from before the start is recorded.
        if (m_Processor->GetCursor() < Handler->GetLatestSequence())
        {
            m_Processor->Consume(Handler->GetLatestSequence());
        }
        m_Elapsed = 0.0;
        m_SourceSequences.clear();
        m_TickCutSequences.clear();
        m_TickCutInbox.Drain([](uint64&) {});
        m_TakesTickCuts.store(true, std::memory_order_release);
        m_State = InputRecorderState::Recording;
        m_Log.Add("Recording started -> " + m_Path);

        // Keys that are held now start the recording as KeyDown messages at time 0.
        for (InputMessage Message :
             InputDeviceStates{}.MakeTransitionMessages(Handler->GetLatestState(), PlatformTime::NowNanoseconds()))
        {
            if (Message.Type == InputMessageType::DeviceConnected ||
                Message.Type == InputMessageType::DeviceDisconnected)
            {
                continue;
            }
            AddMessage(*Handler, Message);
        }

        // So is the UI capture now, also when it is off, so the replay starts from the same capture
        InputMessage Capture;
        Capture.Type = InputMessageType::CaptureChanged;
        Capture.Capture = Handler->GetCapture();
        AddMessage(*Handler, Capture);
    }

    void InputRecorder::AddMessage(const InputMessageHandler& t_Handler, InputMessage t_Message)
    {
        const std::shared_ptr<InputDevice> Device = t_Handler.GetDeviceFactory().Find(t_Message.Device);
        // A capture change has no device
        if (t_Message.Device.IsValid() && m_Recording.FindDevice(t_Message.Device.Value) == nullptr)
        {
            InputRecordingDevice Entry;
            Entry.Id = t_Message.Device.Value;
            if (Device)
            {
                Entry.Info = Device->GetInfo();
                // Playback adds the suffix again, so the file keeps the original name
                String& Name = Entry.Info.DisplayName;
                const StringView Suffix = VirtualInputDeviceInterface::SimulatedNameSuffix;
                if (Device->IsSimulated() && Name.size() >= Suffix.size() &&
                    Name.compare(Name.size() - Suffix.size(), Suffix.size(), Suffix) == 0)
                {
                    Name.erase(Name.size() - Suffix.size());
                }
            }
            m_Recording.Devices.push_back(std::move(Entry));
        }

        InputRecordedMessage Recorded;
        // The frame clock, not TimeNs: under --null-render a whole run takes a few
        // milliseconds of real time, but each tick is still one tick length of frame time
        Recorded.TimeSeconds = m_Elapsed;
        m_SourceSequences.push_back(t_Message.Sequence);
        t_Message.Sequence = 0;
        t_Message.TimeNs = 0;
        t_Message.Simulated = false;
        // The handler stamps the capture again on playback. Only a capture change keeps it
        if (t_Message.Type != InputMessageType::CaptureChanged)
        {
            t_Message.Capture = InputCapture::None;
        }
        Recorded.Message = t_Message;

        const InputRecordingDevice* Entry = m_Recording.FindDevice(t_Message.Device.Value);
        String Name = "Input";
        if (Entry != nullptr)
        {
            Name = Entry->Info.DisplayName.empty() ? String(ToString(Entry->Info.Type)) : Entry->Info.DisplayName;
        }
        m_Log.Add(DescribeRecordedMessage(Recorded.TimeSeconds, Name, t_Message));
        m_Recording.Messages.push_back(std::move(Recorded));
    }

    void InputRecorder::ReadMessages()
    {
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler || !m_Processor)
        {
            return;
        }

        if (m_Processor->Overran())
        {
            m_Log.Add("Messages were lost: the recorder fell behind");
            const InputStateSnapshot Latest = Handler->GetLatestSnapshot();
            m_Processor->Resync(Latest.States, Latest.Sequence);
        }

        constexpr uint32 ChunkSize = 64;
        InputMessage Chunk[ChunkSize];
        while (true)
        {
            const uint32 Got = m_Processor->Read(Chunk, ChunkSize);
            if (Got == 0)
            {
                break;
            }
            for (uint32 i = 0; i < Got; ++i)
            {
                m_Processor->Apply(Chunk[i]);
                if (Chunk[i].Type != InputMessageType::DeviceConnected &&
                    Chunk[i].Type != InputMessageType::DeviceDisconnected)
                {
                    AddMessage(*Handler, Chunk[i]);
                }
            }
        }
    }

    void InputRecorder::TrimStopClick()
    {
        std::vector<InputRecordedMessage>& Messages = m_Recording.Messages;
        for (size_t Index = Messages.size(); Index > 0; --Index)
        {
            const InputMessage& Message = Messages[Index - 1].Message;
            if (Message.Type == InputMessageType::KeyDown && Message.Key == Keys::MouseLeft)
            {
                Messages.erase(Messages.begin() + static_cast<std::ptrdiff_t>(Index - 1), Messages.end());
                m_Log.Add("Removed the click that stopped the recording");
                return;
            }
            const bool IsMouseMessage = Message.Type == InputMessageType::MouseMove ||
                                        Message.Type == InputMessageType::MouseWheel ||
                                        Message.Key.HasFlag(InputKeyFlags::Mouse);
            if (!IsMouseMessage)
            {
                return;
            }
        }
    }

    void InputRecorder::NoteTickCut(const InputCut& t_Cut)
    {
        if (m_TakesTickCuts.load(std::memory_order_acquire))
        {
            m_TickCutInbox.Push(t_Cut.LastSequence);
        }
    }

    void InputRecorder::TakeTickCuts()
    {
        m_TickCutInbox.Drain([this](const uint64 t_Cut) { m_TickCutSequences.push_back(t_Cut); });
    }

    bool InputRecorder::FinishRecording()
    {
        m_TakesTickCuts.store(false, std::memory_order_release);
        TakeTickCuts();
        m_Recording.Duration = m_Elapsed;
        m_Processor.reset();

        // Each tick's cut as a count of recorded messages. They are in queue order, so a
        // cut is the number of recorded messages with a sequence at or before it.
        m_SourceSequences.resize(m_Recording.Messages.size());
        m_Recording.TickCuts.clear();
        for (const uint64 Cut : m_TickCutSequences)
        {
            const auto End = std::upper_bound(m_SourceSequences.begin(), m_SourceSequences.end(), Cut);
            m_Recording.TickCuts.push_back(static_cast<uint64>(End - m_SourceSequences.begin()));
        }
        // The cuts replay only at the rate of the ticks they came from
        m_Recording.TickCutRate = m_Recording.TickCuts.empty() ? 0 : m_TickRate;
        m_Recording.TickRate = m_TickRate;
        m_State = InputRecorderState::Idle;

        String Error;
        if (SaveInputRecording(m_Path, m_Recording, Error))
        {
            m_LastSavedPath = m_Path;
            m_LastError.clear();
            ++m_SaveCount;
            m_Log.Add("Saved " + std::to_string(m_Recording.Messages.size()) + " messages to " + m_Path);
            return true;
        }
        m_LastError = Error;
        m_Log.Add("Save failed: " + Error);
        return false;
    }
} // namespace Fling
#endif // WITH_INPUT_RECORDING
