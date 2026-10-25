#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputMessage.h"
#include <deque>
#include <string>
#include <string_view>
#include <vector>

// Not in shipping builds (see WITH_INPUT_RECORDING in Core/CMakeLists.txt)
#if WITH_INPUT_RECORDING
namespace Fling
{
    /**
     * A device in an input recording. Id is local to the file: messages refer to it, and
     * InputPlayback maps it to a new virtual device when it plays the file.
     */
    struct InputRecordingDevice
    {
        uint32 Id = 0;

        InputDeviceInfo Info;
    };

    /** One message in an input recording, at a time from the start of the recording. */
    struct InputRecordedMessage
    {
        /** Seconds from the start of the recording. Never less than 0. */
        double TimeSeconds = 0.0;

        /**
         * The message. Message.Device holds an InputRecordingDevice::Id, not a live
         * device ID. Sequence and TimeNs are not used.
         */
        InputMessage Message;
    };

    /**
     * A recorded or hand-written input stream. The file format is JSON Lines
     * (*.input.jsonl). See Skills/input-playback.md.
     */
    struct InputRecording
    {
        static constexpr const char* FormatName = "fling-input";

        static constexpr uint32 CurrentVersion = 1;

        /** The tick rate that converts a "tick" field to seconds. */
        static constexpr uint32 DefaultTickRate = 60;

        uint32 Version = CurrentVersion;

        uint32 TickRate = DefaultTickRate;

        /** The duration from the header, in seconds. 0 if the file gives none. */
        double Duration = 0.0;

        /** Every device that a message refers to, with a unique Id. */
        std::vector<InputRecordingDevice> Devices;

        /** Every message, sorted by TimeSeconds. Messages at the same time keep their file order. */
        std::vector<InputRecordedMessage> Messages;

        /** The larger of Duration and the time of the last message. */
        double GetLength() const;

        /** The device with this file Id, or null. */
        const InputRecordingDevice* FindDevice(uint32 t_Id) const;
    };

    /**
     * Read an input recording from JSON Lines text. Blank lines and lines that start
     * with '#' are skipped. Messages that name no device, or name a device type
     * ("Keyboard", "Mouse", "Gamepad"), get a device that is added to t_Out.Devices.
     * A "Press" message becomes a KeyDown and a KeyUp. The result is sorted by time.
     *
     * @param t_Text        The file contents
     * @param t_Out         Receives the recording
     * @param t_OutError    Receives "line N: <reason>" for the first line that is not valid
     * @return False if the text is not a valid recording
     */
    bool ParseInputRecording(std::string_view t_Text, InputRecording& t_Out, std::string& t_OutError);

    /** Read an input recording from a file. See ParseInputRecording. */
    bool LoadInputRecording(const std::string& t_Path, InputRecording& t_Out, std::string& t_OutError);

    /** Write an input recording as JSON Lines text: a header line, then one line for each message. */
    std::string WriteInputRecording(const InputRecording& t_Recording);

    /** Write an input recording to a file. Makes the parent folders if they do not exist. */
    bool SaveInputRecording(const std::string& t_Path, const InputRecording& t_Recording, std::string& t_OutError);

    /**
     * A one-line description of a recorded message for the recorder and playback logs,
     * for example "1.250s  Keyboard  KeyDown W".
     */
    std::string DescribeRecordedMessage(double t_TimeSeconds, std::string_view t_DeviceName, const InputMessage& t_Message);

    /** The newest lines that the recorder or the playback logged. Old lines are removed first. */
    class InputEventLog
    {
    public:
        static constexpr size_t DefaultMaxLines = 512;

        explicit InputEventLog(size_t t_MaxLines = DefaultMaxLines) : m_MaxLines(t_MaxLines) {}

        void Add(std::string t_Line);

        void Clear();

        const std::deque<std::string>& GetLines() const { return m_Lines; }

        /** The number of lines added since the last Clear, including lines that were removed. */
        uint64 GetTotalAdded() const { return m_TotalAdded; }

    private:
        std::deque<std::string> m_Lines;

        size_t m_MaxLines = DefaultMaxLines;

        uint64 m_TotalAdded = 0;
    };
} // namespace Fling
#endif // WITH_INPUT_RECORDING
