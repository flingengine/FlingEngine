#include "Input/InputMessageHandler.h"
#include "Input/InputPlayback.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Logger.h"
#include "PlatformTime.h"
#include "Strings/String.h"

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

        std::atomic<uint64> g_CutTableLocks{0};
    } // namespace

    bool InputPlayback::Start(const StringView t_Path, float t_StartDelay, String& t_OutError)
    {
        InputRecording Recording;
        if (!LoadInputRecording(t_Path, Recording, t_OutError))
        {
            m_Log.Add("Cannot play: " + t_OutError);
            return false;
        }
        if (!Start(std::move(Recording), t_StartDelay, t_OutError))
        {
            t_OutError = String(t_Path) + ": " + t_OutError;
            return false;
        }
        m_Path = t_Path;
        return true;
    }

    bool InputPlayback::Start(InputRecording t_Recording, float t_StartDelay, String& t_OutError)
    {
        if (!CheckTickCutRate(t_Recording, m_TickRate, t_OutError))
        {
            m_Log.Add("Cannot play: " + t_OutError);
            return false;
        }

        if (IsActive())
        {
            Stop();
        }

        m_Recording = std::move(t_Recording);
        m_TickOverran.store(false, std::memory_order_release);
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
            return true;
        }

        BeginPlayback();
        if (m_State == InputPlaybackState::Playing)
        {
            m_IsFirstUpdate = true;
        }
        return true;
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

        // Each file message is queued with its index + 1, so a tick cut can find its sequence
        m_FileSequences.assign(m_Recording.Messages.size(), 0);
        m_Delivered = 0;
        m_LastDeliveredSequence = 0;
        m_ResolvedCuts = 0;
        ResetCutTable(true);
        m_Interface->SetOnSent([this](const uint64 t_Tag, const uint64 t_Sequence)
                               { OnFileMessageSent(t_Tag, t_Sequence); });

        // The replay starts with no UI capture, whatever the live UI has now (live capture
        // changes are ignored while it plays). The file's own CaptureChanged messages change
        // it later. No tag: it is not a file message.
        if (Handler->GetCapture() != InputCapture::None)
        {
            InputMessage StartCapture;
            StartCapture.Type = InputMessageType::CaptureChanged;
            StartCapture.Capture = InputCapture::None;
            m_Interface->Queue(StartCapture);
        }
        m_DeviceMap.clear();
        for (const InputRecordingDevice& Device : m_Recording.Devices)
        {
            InputDeviceInfo Info = Device.Info;
            // Devices of one file must never share an identity.
            Info.HardwarePath = "playback:" + std::to_string(Device.Id);
            if (Info.DisplayName.empty())
            {
                Info.DisplayName = String("Playback ") + ToString(Info.Type);
            }
            m_DeviceMap[Device.Id] = m_Interface->CreateDevice(Info);
        }

        m_State = InputPlaybackState::Playing;
        char Buffer[64];
        std::snprintf(
            Buffer, sizeof(Buffer), " (%zu messages, %.3f s)", m_Recording.Messages.size(), m_Recording.GetLength());
        m_Log.Add("Playing " + (m_Path.empty() ? String("recording") : m_Path) + Buffer);
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
        ResetCutTable(false);
        m_State = InputPlaybackState::Idle;
        m_IsFirstUpdate = false;
        m_Log.Add("Stopped");
    }

    bool InputPlayback::SetTickRate(const uint32 t_Hz)
    {
        if (t_Hz == m_TickRate)
        {
            return true;
        }
        if (IsActive())
        {
            m_Log.Add("Tick rate change to " + std::to_string(t_Hz) + " Hz refused: a replay plays at one rate");
            F_LOG_WARN(
                "Input playback: tick rate change to {} Hz refused while a replay plays at {} Hz", t_Hz, m_TickRate);
            return false;
        }
        m_TickRate = t_Hz;
        return true;
    }

    void InputPlayback::NoteTickOverran()
    {
        if (IsActive())
        {
            m_TickOverran.store(true, std::memory_order_release);
        }
    }

    bool InputPlayback::StopIfOverran()
    {
        if (m_State != InputPlaybackState::Playing || !HasOverrun())
        {
            return false;
        }
        const std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
        const uint32 Capacity = Handler ? Handler->GetQueue().Capacity() : 0;
        const String Error = "a tick lost input: more messages than the input queue holds (" +
                             std::to_string(Capacity) +
                             ") were sent before a tick took them, so the simulation is no longer the recording's";
        m_Log.Add("Replay failed: " + Error);
        F_LOG_ERROR("Input replay of {} stopped: {}", m_Path.empty() ? String("a recording") : m_Path, Error);
        Stop();
        return true;
    }

    void InputPlayback::Update(float t_DeltaSeconds)
    {
        if (StopIfOverran())
        {
            return;
        }
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
                    SendDueMessages(m_Time);
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
        // With per-tick cuts, also wait until every recorded tick has taken its input
        const Detail::PlaybackCutTable* Cuts = m_CutTable.get();
        const bool AllCutsClaimed = !Cuts || Cuts->Claimed.load(std::memory_order_acquire) >= Cuts->Counts.size();
        if (m_NextMessage == m_Recording.Messages.size() && m_Time >= m_Recording.GetLength() - TimeEpsilon &&
            AllCutsClaimed)
        {
            ++m_FinishedCount;
            m_Log.Add("Finished");
            Stop();
            return;
        }

        m_LastDelta = t_DeltaSeconds;
        if (m_IsFirstUpdate)
        {
            m_IsFirstUpdate = false;
        }
        else
        {
            m_Time += static_cast<double>(t_DeltaSeconds);
        }
        SendDueMessages(m_Time);
        SendClaimedCuts();
    }

    void InputPlayback::UpdateLate()
    {
        if (StopIfOverran() || m_State != InputPlaybackState::Playing)
        {
            return;
        }
        // The same sum as the next Update makes, so the two agree to the last bit
        SendDueMessages(m_IsFirstUpdate ? m_Time : m_Time + static_cast<double>(m_LastDelta));
        SendClaimedCuts();
    }

    void InputPlayback::SendDueMessages(const double t_Until)
    {
        if (!m_Interface)
        {
            return;
        }
        while (m_NextMessage < m_Recording.Messages.size() &&
               m_Recording.Messages[m_NextMessage].TimeSeconds <= t_Until + TimeEpsilon)
        {
            SendNext();
        }
    }

    void InputPlayback::SendNext()
    {
        const InputRecordedMessage& Recorded = m_Recording.Messages[m_NextMessage];
        ++m_NextMessage;

        // A capture change has no device
        InputMessage Message = Recorded.Message;
        const auto It = m_DeviceMap.find(Recorded.Message.Device.Value);
        const bool HasDevice = It != m_DeviceMap.end();
        if (HasDevice)
        {
            Message.Device = It->second;
        }
        Message.Sequence = 0;
        Message.TimeNs = 0;
        if ((!HasDevice && Message.Type != InputMessageType::CaptureChanged) ||
            !m_Interface->Queue(Message, m_NextMessage))
        {
            OnFileMessageSent(m_NextMessage, 0);
            return;
        }

        const InputRecordingDevice* Device = m_Recording.FindDevice(Recorded.Message.Device.Value);
        const String Name =
            (Device == nullptr || Device->Info.DisplayName.empty()) ? String("Playback") : Device->Info.DisplayName;
        m_Log.Add(DescribeRecordedMessage(Recorded.TimeSeconds, Name, Recorded.Message));
    }

    std::optional<uint64> InputPlaybackCut::GetSequence() const
    {
        const uint64 Sequence = m_Table->Sequences[m_Index].load(std::memory_order_acquire);
        if (Sequence == Detail::PlaybackCutTable::NotSent)
        {
            return std::nullopt;
        }
        return Sequence;
    }

    std::optional<InputPlaybackCut> InputPlayback::ClaimTickCut()
    {
        if (!m_HasTickCutTable.load(std::memory_order_acquire))
        {
            return std::nullopt;
        }
        std::shared_ptr<Detail::PlaybackCutTable> Cuts;
        {
            ScopedLock Lock(m_TickCutTableMutex);
            g_CutTableLocks.fetch_add(1, std::memory_order_relaxed);
            Cuts = m_TickCutTable;
        }
        if (!Cuts)
        {
            return std::nullopt;
        }
        // Never claim past the last cut, so Claimed stays at most the number of cuts
        size_t Index = Cuts->Claimed.load(std::memory_order_relaxed);
        do
        {
            if (Index >= Cuts->Counts.size())
            {
                return std::nullopt;
            }
        } while (!Cuts->Claimed.compare_exchange_weak(Index, Index + 1, std::memory_order_acq_rel));
        return InputPlaybackCut(Cuts, Index);
    }

    void InputPlayback::SendClaimedCuts()
    {
        const Detail::PlaybackCutTable* Cuts = m_CutTable.get();
        if (m_State != InputPlaybackState::Playing || !m_Interface || !Cuts)
        {
            return;
        }
        const size_t Claimed = std::min(Cuts->Claimed.load(std::memory_order_acquire), Cuts->Counts.size());
        if (Claimed <= m_ResolvedCuts)
        {
            return;
        }

        // The ticks need messages that the clock has not sent yet (this run is slower
        // than the recording): send them now, before the ticks build their input
        const uint64 Count = std::min<uint64>(Cuts->Counts[Claimed - 1], m_Recording.Messages.size());
        while (m_NextMessage < Count)
        {
            SendNext();
        }
        // Messages that are queued but not handled yet go out now too
        ResolveCuts();
        if (Claimed > m_ResolvedCuts)
        {
            m_Interface->Poll(PlatformTime::NowNanoseconds());
        }
    }

    void InputPlayback::OnFileMessageSent(const uint64 t_Tag, const uint64 t_Sequence)
    {
        if (t_Tag >= 1 && t_Tag <= m_FileSequences.size())
        {
            m_FileSequences[t_Tag - 1] = t_Sequence != 0 ? t_Sequence : Dropped;
            ResolveCuts();
        }
    }

    void InputPlayback::ResolveCuts()
    {
        Detail::PlaybackCutTable* Cuts = m_CutTable.get();
        if (!Cuts)
        {
            return;
        }
        const auto PublishReady = [&]()
        {
            while (m_ResolvedCuts < Cuts->Counts.size() && Cuts->Counts[m_ResolvedCuts] <= m_Delivered)
            {
                Cuts->Sequences[m_ResolvedCuts].store(m_LastDeliveredSequence, std::memory_order_release);
                ++m_ResolvedCuts;
            }
        };
        PublishReady();
        while (m_Delivered < m_FileSequences.size() && m_FileSequences[m_Delivered] != 0)
        {
            if (m_FileSequences[m_Delivered] != Dropped)
            {
                m_LastDeliveredSequence = m_FileSequences[m_Delivered];
            }
            ++m_Delivered;
            PublishReady();
        }
    }

    void InputPlayback::ResetCutTable(const bool t_ForNewPlayback)
    {
        std::shared_ptr<Detail::PlaybackCutTable> Table;
        if (t_ForNewPlayback && !m_Recording.TickCuts.empty())
        {
            Table = std::make_shared<Detail::PlaybackCutTable>();
            Table->Counts = m_Recording.TickCuts;
            Table->Sequences = std::make_unique<std::atomic<uint64>[]>(Table->Counts.size());
            for (size_t i = 0; i < Table->Counts.size(); ++i)
            {
                Table->Sequences[i].store(Detail::PlaybackCutTable::NotSent, std::memory_order_relaxed);
            }
        }
        if (m_CutTable)
        {
            m_CutTable->Abandoned.store(true, std::memory_order_release);
        }
        m_CutTable = std::move(Table);
        if (!m_CutTable && !m_HasTickCutTable.load(std::memory_order_relaxed))
        {
            return;
        }
        ScopedLock Lock(m_TickCutTableMutex);
        g_CutTableLocks.fetch_add(1, std::memory_order_relaxed);
        m_TickCutTable = m_CutTable;
        m_HasTickCutTable.store(m_CutTable != nullptr, std::memory_order_release);
    }

    uint64 InputPlayback::GetCutTableLockCount()
    {
        return g_CutTableLocks.load(std::memory_order_relaxed);
    }
} // namespace Fling
#endif // WITH_INPUT_RECORDING
