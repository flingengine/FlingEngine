#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Input/TickInput.h"
#include "Threading/Lane.h"
#include "TickTimeline.h"

#include <functional>
#include <memory>
#include <optional>

namespace Fling
{
    class Input;
    class InputMessageHandler;
    class InputPlayback;
    class InputRecorder;

    /** Who sends the playback messages that a recorded cut needs when the replay is slower than the recording. */
    enum class TickCutSender : uint8
    {
        /** The tick lane polls input itself (one thread, as in tests): it sends them. */
        TickLane,

        /**
         * Another thread polls: a Build on the tick lane waits until that thread's next poll
         * sends them. A Build on any other lane runs on the writer's thread (an editor tick
         * outside play), so it sends them itself.
         */
        Writer,
    };

    struct TickInputProviderConfig
    {
        TickCutSender CutSender = TickCutSender::TickLane;

        /**
         * The lane of the thread that calls Build. With TickCutSender::TickLane, debug builds
         * check it (None checks nothing). With TickCutSender::Writer, a Build on this lane is
         * on another thread than the writer, and only such a Build waits.
         */
        Lane TickLane = Lane::None;

        /**
         * TickCutSender::Writer, a timed tick on the tick lane: wait at most this long for the
         * writer's watermark (InputMessageHandler::GetWatermark) to reach the end of the
         * window, so the tick sees every message of its window. 0: no wait, the tick takes
         * what is in the queue (live play). Negative: no limit (a replay).
         */
        int64 WatermarkWaitNs = 0;

        /**
         * TickCutSender::Writer: every wait for the writer ends when this returns true (the
         * tick lane is asked to stop, so the writer may never poll again). Any thread.
         */
        std::function<bool()> StopWaiting;
    };

    /**
     * The one rule that assigns input to a fixed tick, built on the tick lane. The engine
     * loop and every test rig use it. A tick that the clock ran (TickTrigger::Clock) takes
     * the messages before the end of its window (time rule); a tick run on request takes
     * every queued message. A replay of a file with per-tick cuts takes exactly the
     * recorded cut instead, so each tick gets the same input at any frame rate. Each
     * tick's cut goes to the recorder, and a tick that overran the queue during a replay
     * goes to the playback, which then stops with an error. UI capture comes from the
     * stream (InputMessage::Capture).
     *
     * It touches the playback and the recorder only through thread-safe calls
     * (InputPlayback::ClaimTickCut and NoteTickOverran, InputRecorder::NoteTickCut, the
     * queue), so the tick lane can be another thread than the one that polls. Make it
     * after Input::Init, and stop calling Build before Input::Shutdown.
     */
    class CORE_API TickInputProvider
    {
    public:
        /** The Input singleton's handler, playback and recorder. */
        explicit TickInputProvider(Input& t_Input, const TickInputProviderConfig& t_Config = {});

        /** A handler with its own playback and recorder (either may be null). They must outlive the provider. */
        TickInputProvider(std::shared_ptr<const InputMessageHandler> t_Handler,
                          InputPlayback* t_Playback,
                          InputRecorder* t_Recorder,
                          const TickInputProviderConfig& t_Config = {});

        /** Fill t_Out with the input of the tick of t_Window. */
        void Build(TickInput& t_Out, const TickWindow& t_Window);

        /** The cut of the last Build. */
        uint64 GetCursor() const { return m_Builder.GetCursor(); }

    private:
        /** The sequence of this tick's recorded cut, or nothing when the tick takes its input by time. */
        std::optional<uint64> TakeRecordedCut(bool t_WriterIsOtherThread);

        /**
         * Wait until the writer's watermark reaches t_EndNs, the wait budget ends, or
         * StopWaiting. It sleeps until a poll (InputMessageHandler::WaitForPoll): no spin.
         */
        void WaitForWatermark(int64 t_EndNs) const;

        /** Sleep until the writer's next poll, or at most a short time (StopWaiting is checked between). */
        void WaitForWriter() const;

        bool ShouldStopWaiting() const { return m_Config.StopWaiting && m_Config.StopWaiting(); }

        TickInputProviderConfig m_Config;

        TickInputBuilder m_Builder;

        /** For the watermark */
        std::shared_ptr<const InputMessageHandler> m_Handler;

        InputPlayback* m_Playback = nullptr;

        InputRecorder* m_Recorder = nullptr;
    };
} // namespace Fling
