#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputRecording.h"
#include <memory>
#include <string>
#include <unordered_map>

namespace Fling
{
    class InputMessageHandler;
    class VirtualInputDeviceInterface;

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
     * Under --null-render every tick is 1/60 s, so a run is the same every time.
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
         * @param t_OutError    Receives the reason if the file cannot be loaded
         * @return False if the file cannot be loaded. Nothing plays then
         */
        bool Start(const std::string& t_Path, float t_StartDelay, std::string& t_OutError);

        /** Start playback of a recording that is in memory. */
        void Start(InputRecording t_Recording, float t_StartDelay);

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

        InputPlaybackState GetState() const { return m_State; }

        bool IsActive() const { return m_State != InputPlaybackState::Idle; }

        float GetCountdownRemaining() const { return m_CountdownRemaining; }

        float GetCountdownTotal() const { return m_CountdownTotal; }

        /** Seconds of playback so far. */
        double GetTime() const { return m_Time; }

        /** The length of the recording that plays, in seconds. */
        double GetLength() const { return m_Recording.GetLength(); }

        /** The file that plays now or played last. Empty for a recording from memory. */
        const std::string& GetPath() const { return m_Path; }

        /** The number of messages sent so far, and the total. */
        size_t GetSentCount() const { return m_NextMessage; }

        size_t GetMessageCount() const { return m_Recording.Messages.size(); }

        /** Increases by 1 each time playback gets to the end, so callers can wait for it. */
        uint32 GetFinishedCount() const { return m_FinishedCount; }

        /** One line for each message that was sent, and lines for start and stop. */
        InputEventLog& GetLog() { return m_Log; }

        const InputEventLog& GetLog() const { return m_Log; }

    private:
        /** Add the "Playback" interface and make one device for each recording device. */
        void BeginPlayback();

        void SendDueMessages();

        std::weak_ptr<InputMessageHandler> m_Handler;

        std::shared_ptr<VirtualInputDeviceInterface> m_Interface;

        InputInterfaceId m_InterfaceId;

        /** Recording device Id to the live device. */
        std::unordered_map<uint32, InputDeviceId> m_DeviceMap;

        InputRecording m_Recording;

        InputEventLog m_Log;

        std::string m_Path;

        InputPlaybackState m_State = InputPlaybackState::Idle;

        float m_CountdownRemaining = 0.0f;

        float m_CountdownTotal = 0.0f;

        double m_Time = 0.0;

        size_t m_NextMessage = 0;

        uint32 m_FinishedCount = 0;

        /** True until the first Update of playback, which keeps the clock at 0. */
        bool m_IsFirstUpdate = false;
    };
} // namespace Fling
