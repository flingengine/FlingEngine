#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceTypes.h"
#include "Input/InputMessage.h"
#include "Strings/String.h"

#include <deque>
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

        /**
         * The version that WriteInputRecording writes. Version 2 stores the tick rate of the
         * tick cuts ({"tickCuts":{"tickRate":30,"counts":[...]}}). Version 1 files still load.
         */
        static constexpr uint32 CurrentVersion = 2;

        /**
         * The tick rate that converts a "tick" field to seconds when the header gives none. A
         * format default for hand-written files, not the simulation rate.
         */
        static constexpr uint32 DefaultTickRate = 60;

        /**
         * The rate of the tick cuts of a version 1 file, which did not store it: every version 1
         * recording was made at 60 Hz, the only simulation rate then.
         */
        static constexpr uint32 Version1TickCutRate = 60;

        uint32 Version = CurrentVersion;

        /** Ticks a second for the "tick" fields of messages (the header's "tickRate"). */
        uint32 TickRate = DefaultTickRate;

        /** The duration from the header, in seconds. 0 if the file gives none. */
        double Duration = 0.0;

        /** Every device that a message refers to, with a unique Id. */
        std::vector<InputRecordingDevice> Devices;

        /** Every message, sorted by TimeSeconds. Messages at the same time keep their file order. */
        std::vector<InputRecordedMessage> Messages;

        /**
         * The input cut of each tick that ran while recording, in tick order: entry k is
         * how many of Messages ticks 0..k consumed together (never decreasing, at most
         * Messages.size()). A replay gives tick k exactly those messages, so it gets the
         * same per-tick input at any frame rate (TickInputProvider). Empty for a
         * hand-written file or an old recording: those replay by time.
         */
        std::vector<uint64> TickCuts;

        /**
         * The simulation rate, in ticks a second, of the ticks that TickCuts were recorded on.
         * Set whenever TickCuts is not empty. Cuts are per tick, so they replay only at this
         * rate (CheckTickCutRate).
         */
        uint32 TickCutRate = 0;

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
    bool ParseInputRecording(StringView t_Text, InputRecording& t_Out, String& t_OutError);

    /** Read an input recording from a file. See ParseInputRecording. */
    bool LoadInputRecording(StringView t_Path, InputRecording& t_Out, String& t_OutError);

    /**
     * Write an input recording as JSON Lines text, in the CurrentVersion format: a header line,
     * one line for each message, then the tick cuts with their rate (if it has cuts).
     */
    String WriteInputRecording(const InputRecording& t_Recording);

    /**
     * True if a run at t_SimHz ticks a second can replay the recording: it has no tick cuts
     * (it replays by time, at any rate), or its cuts were recorded at t_SimHz. A replay at
     * another rate would give each tick the input of a different time, so it is refused.
     *
     * @param t_OutError    Receives a message with both rates and the flag that replays it
     */
    bool CheckTickCutRate(const InputRecording& t_Recording, uint32 t_SimHz, String& t_OutError);

    /** Write an input recording to a file. Makes the parent folders if they do not exist. */
    bool SaveInputRecording(StringView t_Path, const InputRecording& t_Recording, String& t_OutError);

    /**
     * A one-line description of a recorded message for the recorder and playback logs,
     * for example "1.250s  Keyboard  KeyDown W".
     */
    String DescribeRecordedMessage(double t_TimeSeconds, StringView t_DeviceName, const InputMessage& t_Message);

    /** The newest lines that the recorder or the playback logged. Old lines are removed first. */
    class InputEventLog
    {
    public:
        static constexpr size_t DefaultMaxLines = 512;

        explicit InputEventLog(size_t t_MaxLines = DefaultMaxLines) : m_MaxLines(t_MaxLines) {}

        void Add(String t_Line);

        void Clear();

        const std::deque<String>& GetLines() const { return m_Lines; }

        /** The number of lines added since the last Clear, including lines that were removed. */
        uint64 GetTotalAdded() const { return m_TotalAdded; }

    private:
        std::deque<String> m_Lines;

        size_t m_MaxLines = DefaultMaxLines;

        uint64 m_TotalAdded = 0;
    };
} // namespace Fling
#endif // WITH_INPUT_RECORDING
