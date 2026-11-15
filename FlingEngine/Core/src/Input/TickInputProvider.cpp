#include "Input/Input.h"
#include "Input/TickInputProvider.h"
#include "MathUtils.h"
#include "PlatformTime.h"

#include <chrono>
#include <limits>
#include <thread>

namespace Fling
{
    namespace
    {
        /**
         * The longest sleep of a tick that waits for the writer: a poll wakes it sooner. It
         * only bounds how long a stop (StopWaiting) or an abandoned replay cut takes to be seen.
         */
        constexpr std::chrono::milliseconds LongestWriterWait{10};

#if WITH_INPUT_RECORDING
        InputPlayback* PlaybackOf(Input& t_Input)
        {
            return t_Input.IsInitialized() ? &t_Input.GetPlayback() : nullptr;
        }

        InputRecorder* RecorderOf(Input& t_Input)
        {
            return t_Input.IsInitialized() ? &t_Input.GetRecorder() : nullptr;
        }
#else
        InputPlayback* PlaybackOf(Input&)
        {
            return nullptr;
        }

        InputRecorder* RecorderOf(Input&)
        {
            return nullptr;
        }
#endif
    } // namespace

    TickInputProvider::TickInputProvider(Input& t_Input, const TickInputProviderConfig& t_Config)
        : TickInputProvider(t_Input.GetHandler(), PlaybackOf(t_Input), RecorderOf(t_Input), t_Config)
    {
    }

    TickInputProvider::TickInputProvider(std::shared_ptr<const InputMessageHandler> t_Handler,
                                         InputPlayback* const t_Playback,
                                         InputRecorder* const t_Recorder,
                                         const TickInputProviderConfig& t_Config)
        : m_Config(t_Config),
          m_Builder(t_Handler),
          m_Handler(std::move(t_Handler)),
          m_Playback(t_Playback),
          m_Recorder(t_Recorder)
    {
    }

    void TickInputProvider::Build(TickInput& t_Out, const TickWindow& t_Window)
    {
        // With a writer on another thread, only a Build on the tick lane runs beside it.
        // Elsewhere (an editor tick outside play) this thread is the writer.
        const bool WriterIsOtherThread =
            m_Config.CutSender == TickCutSender::Writer && LaneCheck::Current() == m_Config.TickLane;
        if (m_Config.CutSender == TickCutSender::TickLane)
        {
            FLING_LANE_CHECK(m_Config.TickLane);
        }

        const bool Timed = t_Window.Trigger == TickTrigger::Clock;
        TickInputLimit Limit;
        if (const std::optional<uint64> Cut = TakeRecordedCut(WriterIsOtherThread))
        {
            Limit.UntilSequence = *Cut;
        }
        else if (Timed)
        {
            Limit.UntilNs = t_Window.EndNs;
            if (WriterIsOtherThread)
            {
                WaitForWatermark(t_Window.EndNs);
            }
        }
        m_Builder.Build(t_Out, Limit);
#if WITH_INPUT_RECORDING
        if (m_Recorder)
        {
            m_Recorder->NoteTickCut(t_Out.Slice.Cut);
        }
        // A replay that lost input is no longer the recording: the playback reports it
        if (m_Playback && t_Out.Slice.Overran)
        {
            m_Playback->NoteTickOverran();
        }
#endif
    }

    void TickInputProvider::WaitForWatermark(const int64 t_EndNs) const
    {
        if (m_Config.WatermarkWaitNs == 0 || !m_Handler)
        {
            return;
        }
        const int64 GiveUpNs = m_Config.WatermarkWaitNs < 0 ? std::numeric_limits<int64>::max()
                                                            : PlatformTime::NowNanoseconds() + m_Config.WatermarkWaitNs;
        while (m_Handler->GetWatermark() < t_EndNs && !ShouldStopWaiting())
        {
            const int64 LeftNs = GiveUpNs - PlatformTime::NowNanoseconds();
            if (LeftNs <= 0)
            {
                return;
            }
            m_Handler->WaitForPoll(
                Math::Min<std::chrono::nanoseconds>(std::chrono::nanoseconds(LeftNs), LongestWriterWait));
        }
    }

    void TickInputProvider::WaitForWriter() const
    {
        if (m_Handler)
        {
            m_Handler->WaitForPoll(LongestWriterWait);
        }
        else
        {
            std::this_thread::sleep_for(LongestWriterWait);
        }
    }

    std::optional<uint64> TickInputProvider::TakeRecordedCut(const bool t_WriterIsOtherThread)
    {
#if WITH_INPUT_RECORDING
        if (!m_Playback)
        {
            return std::nullopt;
        }
        const std::optional<InputPlaybackCut> Cut = m_Playback->ClaimTickCut();
        if (!Cut)
        {
            return std::nullopt;
        }
        if (!t_WriterIsOtherThread)
        {
            m_Playback->SendClaimedCuts();
            return Cut->GetSequence();
        }
        // The writer sends the cut at its next poll. A replay waits for it: exact input
        // per tick matters more than latency there.
        while (true)
        {
            if (const std::optional<uint64> Sequence = Cut->GetSequence())
            {
                return Sequence;
            }
            if (Cut->IsAbandoned() || ShouldStopWaiting())
            {
                return std::nullopt;
            }
            WaitForWriter();
        }
#else
        (void)t_WriterIsOtherThread;
        return std::nullopt;
#endif
    }
} // namespace Fling
