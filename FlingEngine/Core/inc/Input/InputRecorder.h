#pragma once

#include "FlingTypes.h"
#include "Input/InputMessageProcessor.h"
#include "Input/InputRecording.h"
#include <memory>
#include <optional>
#include <string>

// Not in shipping builds (see WITH_INPUT_RECORDING in Core/CMakeLists.txt)
#if WITH_INPUT_RECORDING
namespace Fling
{
    class InputMessageHandler;

    enum class InputRecorderState : uint8
    {
        Idle,

        /** Waiting for the start delay to end. */
        Countdown,

        Recording,
    };

    /**
     * Records the input message stream to a file. It is one more consumer of the
     * message queue, with its own InputMessageProcessor, so it does not change what
     * other consumers see.
     *
     * When recording starts, every key that is held then is written as a KeyDown at
     * time 0. Connect and disconnect messages are not written; the file header lists
     * the devices.
     */
    class InputRecorder
    {
    public:
        explicit InputRecorder(std::weak_ptr<InputMessageHandler> t_Handler) : m_Handler(std::move(t_Handler)) {}

        /**
         * Start a recording. A recording that is in progress is stopped and saved first.
         *
         * @param t_Path            The file to write. Relative paths are not changed here
         * @param t_StartDelay      Seconds to wait before recording starts
         * @param t_Duration        Seconds to record. 0 means "until Stop"
         */
        void Start(const std::string& t_Path, float t_StartDelay, float t_Duration);

        /**
         * Stop. During the countdown this cancels and writes nothing. During recording this
         * saves the file.
         *
         * @param t_TrimStopClick  True if a MouseLeft click stops the recording, for example
         *                         a UI button. The last MouseLeft press, and the mouse messages
         *                         after it, are not saved, so playback does not click that button
         * @return False if a save failed. GetLastError gives the reason
         */
        bool Stop(bool t_TrimStopClick = false);

        /**
         * Advance the countdown and the recording clock, and read new messages. Call this
         * once per frame, after the interfaces are polled. When the duration ends, the file
         * is saved.
         */
        void Update(float t_DeltaSeconds);

        InputRecorderState GetState() const { return m_State; }

        bool IsActive() const { return m_State != InputRecorderState::Idle; }

        /** Seconds left in the countdown. */
        float GetCountdownRemaining() const { return m_CountdownRemaining; }

        /** The start delay given to Start. */
        float GetCountdownTotal() const { return m_CountdownTotal; }

        /** Seconds since recording started. */
        float GetElapsed() const { return static_cast<float>(m_Elapsed); }

        /** The duration given to Start. 0 means "until Stop". */
        float GetDuration() const { return m_Duration; }

        /** The file that the current or the last recording writes to. */
        const std::string& GetPath() const { return m_Path; }

        /** The number of messages in the current recording. */
        size_t GetMessageCount() const { return m_Recording.Messages.size(); }

        /** The file of the last recording that was saved. Empty if none was saved. */
        const std::string& GetLastSavedPath() const { return m_LastSavedPath; }

        /** Increases by 1 for each saved recording, so UI can see that a new file exists. */
        uint32 GetSaveCount() const { return m_SaveCount; }

        /** The reason the last save failed. Empty if it did not fail. */
        const std::string& GetLastError() const { return m_LastError; }

        /** One line for each message that was recorded, and lines for start, stop and save. */
        InputEventLog& GetLog() { return m_Log; }

        const InputEventLog& GetLog() const { return m_Log; }

    private:
        void BeginRecording();

        bool FinishRecording();

        void ReadMessages();

        /** Remove the last MouseLeft KeyDown and the mouse messages after it, if only mouse messages follow it. */
        void TrimStopClick();

        /** Add a message to the recording (and its device, the first time), and log it. */
        void AddMessage(const InputMessageHandler& t_Handler, InputMessage t_Message);

        std::weak_ptr<InputMessageHandler> m_Handler;

        std::optional<InputMessageProcessor> m_Processor;

        InputRecording m_Recording;

        InputEventLog m_Log;

        std::string m_Path;

        std::string m_LastSavedPath;

        std::string m_LastError;

        InputRecorderState m_State = InputRecorderState::Idle;

        float m_CountdownRemaining = 0.0f;

        float m_CountdownTotal = 0.0f;

        /**
         * The recording clock, moved by the delta time given to Update. A message gets the
         * clock time of the Update that reads it, the same clock that InputPlayback uses.
         */
        double m_Elapsed = 0.0;

        float m_Duration = 0.0f;

        uint32 m_SaveCount = 0;
    };
} // namespace Fling
#endif // WITH_INPUT_RECORDING
