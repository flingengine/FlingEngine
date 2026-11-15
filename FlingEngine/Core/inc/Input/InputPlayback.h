#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputRecording.h"
#include "SimRate.h"
#include "Strings/String.h"
#include "Threading/Mutex.h"

#include <atomic>
#include <memory>
#include <optional>
#include <unordered_map>
#include <vector>

// Not in shipping builds (see WITH_INPUT_RECORDING in Core/CMakeLists.txt)
#if WITH_INPUT_RECORDING
namespace Fling
{
    class InputMessageHandler;
    class VirtualInputDeviceInterface;

    namespace Detail
    {
        /** The per-tick cuts of the file that plays, shared with the tick lane. */
        struct PlaybackCutTable
        {
            static constexpr uint64 NotSent = ~uint64(0);

            /** InputRecording::TickCuts: file message counts. Never changes. */
            std::vector<uint64> Counts;

            /** For each cut, the sequence of its last file message once every message of the cut is sent, else NotSent.
             */
            std::unique_ptr<std::atomic<uint64>[]> Sequences;

            /** How many cuts the ticks have claimed. */
            std::atomic<size_t> Claimed{0};

            /** Set when playback stops: a claimed cut that was not sent will never be. */
            std::atomic<bool> Abandoned{false};
        };
    } // namespace Detail

    /**
     * The recorded cut of one tick, claimed by the tick lane with
     * InputPlayback::ClaimTickCut. Safe to use on any thread, also after playback stops.
     */
    class InputPlaybackCut
    {
    public:
        InputPlaybackCut(std::shared_ptr<const Detail::PlaybackCutTable> t_Table, size_t t_Index)
            : m_Table(std::move(t_Table)), m_Index(t_Index)
        {
        }

        /** The sequence of the cut's last file message: the tick consumes up to it. Empty until the writer has sent all
         * of the cut. */
        std::optional<uint64> GetSequence() const;

        /** True if playback stopped before it sent the cut. Take the input by time then. */
        bool IsAbandoned() const { return m_Table->Abandoned.load(std::memory_order_acquire); }

    private:
        std::shared_ptr<const Detail::PlaybackCutTable> m_Table;
        size_t m_Index = 0;
    };

    enum class InputPlaybackState : uint8
    {
        Idle,

        /** Waiting for the start delay to end. */
        Countdown,

        Playing,
    };

    /**
     * Plays an input recording back through a VirtualInputDeviceInterface named
     * "Playback", so every consumer sees the same messages that a real device would
     * send. It needs no window, so it works with --null-render.
     *
     * The playback clock moves by the delta time given to Update, not by real time.
     * Under --null-render every frame has the same delta (one tick length, or 1/N of it with
     * --frames-per-tick N), so a run is the same every time.
     *
     * Threads: playback sends messages, so it belongs to the writer lane of the input
     * queue (Input::SetWriterLane). ClaimTickCut, NoteTickOverran, HasOverrun and IsActive
     * are thread safe, for the tick lane; the writer sends what the claimed cuts need
     * (SendClaimedCuts).
     *
     * A replay owns its start: the first message it sends is a CaptureChanged with no
     * capture, so the live UI capture of the moment it starts does not apply to it.
     */
    class InputPlayback
    {
    public:
        static constexpr const char* InterfaceName = "Playback";

        explicit InputPlayback(std::weak_ptr<InputMessageHandler> t_Handler) : m_Handler(std::move(t_Handler)) {}

        /**
         * Load a file and start playback. Playback that is in progress is stopped first.
         *
         * @param t_Path        The file to play. Relative paths are not changed here
         * @param t_StartDelay  Seconds to wait before playback starts
         * @param t_OutError    Receives the reason if the file cannot be loaded or played
         * @return False if the file cannot be loaded, or its tick cuts were recorded at another
         *         rate than SetTickRate (CheckTickCutRate). Nothing plays then
         */
        bool Start(StringView t_Path, float t_StartDelay, String& t_OutError);

        /** Start playback of a recording that is in memory. False, and nothing plays, as for a file. */
        bool Start(InputRecording t_Recording, float t_StartDelay, String& t_OutError);

        /**
         * The simulation rate of the ticks that claim the cuts (ClaimTickCut). A recording whose
         * cuts were made at another rate does not start. Set it before Start: while playback is
         * active a change is refused (false, and a log line), because the cuts play at one rate.
         */
        bool SetTickRate(uint32 t_Hz);

        uint32 GetTickRate() const { return m_TickRate; }

        /**
         * Stop playback. The playback devices are disconnected, so their held keys are
         * released, and the "Playback" interface is removed.
         */
        void Stop();

        /**
         * Advance the countdown or the playback clock, and queue every message that is due.
         * Call this once per frame, before the interfaces are polled. On the first Update
         * of playback the clock is 0, so messages at time 0 go out on that frame. When the
         * clock reaches the recording length, playback stops.
         */
        void Update(float t_DeltaSeconds);

        /**
         * The late poll of a frame (Input::PollLate, after the fixed ticks): queue now the
         * messages that the next Update would queue if its delta is the same as the last
         * one. The clock does not move. Under --null-render every delta is the same, so a
         * message goes out here instead of at the next Update, the first tick that runs
         * after it is the same one, and a replay gives the same simulation as before; only
         * the frame systems see the message one frame earlier. With real frame times the
         * next delta is a guess, so a message can be one frame early (never late).
         */
        void UpdateLate();

        /**
         * Tick lane, thread safe: claim the recorded cut of the next tick. Call once for
         * each tick that runs (TickInputProvider does). Empty when there is no cut (no
         * playback, an old file, or more ticks than the recording had): the tick then takes
         * its input by time. The writer sends the messages of the cut at its next Update,
         * UpdateLate or SendClaimedCuts; then InputPlaybackCut::GetSequence is the sequence
         * of the cut's last file message, and the tick consumes up to it
         * (TickInputLimit::UntilSequence).
         */
        std::optional<InputPlaybackCut> ClaimTickCut();

        /**
         * Writer: send at once every file message that the claimed cuts need and the clock
         * has not sent yet (this run is slower than the recording). Update and UpdateLate
         * call it.
         */
        void SendClaimedCuts();

        /**
         * Tick lane, thread safe: a tick of this replay found that the input queue overwrote
         * messages before the tick took them (InputSlice::Overran). The simulation is no
         * longer the recording's, so the next Update or UpdateLate stops playback with an
         * error. Does nothing while playback is not active.
         */
        void NoteTickOverran();

        /** Any thread: how many times any playback took the lock that hands its cuts to the tick lane. */
        static uint64 GetCutTableLockCount();

        /** Any thread: true if a tick of the last playback overran the queue (NoteTickOverran). */
        bool HasOverrun() const { return m_TickOverran.load(std::memory_order_acquire); }

        /** True while playing a file that has per-tick cuts. */
        bool HasTickCuts() const { return IsActive() && !m_Recording.TickCuts.empty(); }

        InputPlaybackState GetState() const { return m_State; }

        bool IsActive() const { return m_State != InputPlaybackState::Idle; }

        float GetCountdownRemaining() const { return m_CountdownRemaining; }

        float GetCountdownTotal() const { return m_CountdownTotal; }

        /** Seconds of playback so far. */
        double GetTime() const { return m_Time; }

        /** The length of the recording that plays, in seconds. */
        double GetLength() const { return m_Recording.GetLength(); }

        /** The file that plays now or played last. Empty for a recording from memory. */
        const String& GetPath() const { return m_Path; }

        /** The number of messages sent so far, and the total. */
        size_t GetSentCount() const { return m_NextMessage; }

        size_t GetMessageCount() const { return m_Recording.Messages.size(); }

        /** Increases by 1 each time playback gets to the end, so callers can wait for it. */
        uint32 GetFinishedCount() const { return m_FinishedCount; }

        /** One line for each message that was sent, and lines for start and stop. */
        InputEventLog& GetLog() { return m_Log; }

        const InputEventLog& GetLog() const { return m_Log; }

    private:
        /** Add the "Playback" interface, make one device for each recording device, and queue the start capture. */
        void BeginPlayback();

        /** Stop with an error if a tick overran the queue (NoteTickOverran). True if it stopped. */
        bool StopIfOverran();

        /** Queue every message with a time up to t_Until (plus a small epsilon). */
        void SendDueMessages(double t_Until);

        /** Queue the next file message on its playback device. */
        void SendNext();

        /** The handler gave file message t_Tag - 1 the sequence t_Sequence (0: it was dropped). */
        void OnFileMessageSent(uint64 t_Tag, uint64 t_Sequence);

        /** Publish the sequence of each cut whose messages are all sent. */
        void ResolveCuts();

        /** Make the cut table of a new playback, or none, and abandon the old one. */
        void ResetCutTable(bool t_ForNewPlayback);

        std::weak_ptr<InputMessageHandler> m_Handler;

        std::shared_ptr<VirtualInputDeviceInterface> m_Interface;

        InputInterfaceId m_InterfaceId;

        /** Recording device Id to the live device. */
        std::unordered_map<uint32, InputDeviceId> m_DeviceMap;

        InputRecording m_Recording;

        InputEventLog m_Log;

        String m_Path;

        /** Atomic so that IsActive is thread safe. Only the writer changes it. */
        std::atomic<InputPlaybackState> m_State{InputPlaybackState::Idle};

        /** @see NoteTickOverran. Cleared by Start. */
        std::atomic<bool> m_TickOverran{false};

        float m_CountdownRemaining = 0.0f;

        float m_CountdownTotal = 0.0f;

        double m_Time = 0.0;

        /** The delta of the last Update, for UpdateLate. */
        float m_LastDelta = 0.0f;

        size_t m_NextMessage = 0;

        uint32 m_FinishedCount = 0;

        /** True until the first Update of playback, which keeps the clock at 0. */
        bool m_IsFirstUpdate = false;

        /** @see SetTickRate */
        uint32 m_TickRate = SimRate::DefaultHz;

        /** The sequence the handler gave each file message (0: not sent yet, Dropped: never sent). */
        std::vector<uint64> m_FileSequences;

        static constexpr uint64 Dropped = ~uint64(0);

        /** File messages 0 .. m_Delivered - 1 are sent or dropped. The sequence of the last one sent. */
        size_t m_Delivered = 0;
        uint64 m_LastDeliveredSequence = 0;

        /** Cuts 0 .. m_ResolvedCuts - 1 have their sequence in the cut table. */
        size_t m_ResolvedCuts = 0;

        /** The cut table of the playback, or none. Only the writer uses this pointer. */
        std::shared_ptr<Detail::PlaybackCutTable> m_CutTable;

        /** True while a playback has a cut table, so that ClaimTickCut takes no lock in live play. */
        std::atomic<bool> m_HasTickCutTable{false};

        // Only a replay with cuts takes this lock: the writer at start and stop, the tick lane once a tick
        Mutex m_TickCutTableMutex;
        std::shared_ptr<Detail::PlaybackCutTable> m_TickCutTable FLING_GUARDED_BY(m_TickCutTableMutex);
    };
} // namespace Fling
#endif // WITH_INPUT_RECORDING
