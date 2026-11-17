#pragma once

#include "ExecMode.h"
#include "FlingTypes.h"
#include "Input/InputMessage.h"
#include "Strings/String.h"

#include <optional>
#include <vector>

namespace Fling
{
    class CommandLine;

    /** The format of log lines on the console and in the log file. */
    enum class LogFormat : uint8
    {
        /** Human-readable text lines. */
        Text,

        /** One JSON object per line, for scripts and agents. */
        Json,
    };

    /**
     * Launch flags that change how the engine runs: headless, unattended, a set
     * number of ticks, and a fixed seed. Parse them from the command line with
     * LaunchOptions::Parse.
     *
     *   --null-render       No window, no swapchain, no GPU work
     *   --offscreen         Render with the GPU into images the engine owns: no window and no
     *                       surface. Timing is the same as --null-render (a virtual clock, no
     *                       frame cap), so a run gives the same ticks. For --screenshot
     *   --resolution WxH    The size of the frames (--offscreen) or of the window, for example 1280x720.
     *                       Overrides [Engine] WindowWidth and WindowHeight. A window is still
     *                       made smaller to fit the screen
     *   --screenshot F      Save the rendered frame as a PNG file F (relative to the project).
     *                       Needs a renderer (--offscreen or a window), not --null-render
     *   --screenshot-ticks L  The ticks to save a frame at, a comma list such as 30,60,120: the
     *                       first frame that shows each tick. With more than one tick, or with
     *                       {tick} in F, each file name gets the tick. Without it: the last tick
     *                       of the run (needs --frames)
     *   --dump-state F      Write the simulation state (every saved component and singleton) as
     *                       JSON to file F, on the game thread right after a tick. Works with
     *                       --null-render
     *   --dump-state-ticks L  The ticks to write the state at (the same rules as --screenshot-ticks)
     *   --unattended        No modal dialogs. A crash prints a stack trace and exits with code 2
     *   --frames N          Run N frames, then exit, in every exec mode. No tick runs after
     *                       tick N / frames-per-tick (rounded up), and the run ends once that
     *                       tick is shown
     *   --frames-per-tick N With --null-render, run one fixed tick every N frames (default 1),
     *                       so frame systems see the input of the frames between ticks.
     *                       --frames still counts frames
     *   --seed S            Seed the random number generator
     *   --log-format json   Write logs as JSON lines
     *   --level F           Load level file F (relative to Assets/) instead of [Game] StartLevel
     *   --log-input [all]   Log each input message. "all" also logs mouse movement.
     *                       Device connects and disconnects are always logged
     *   --replay-input F    Play the input messages in file F from the first tick
     *   --record-input F    Record the input messages, and save them to file F at exit
     *                       (--replay-input and --record-input are errors in shipping builds)
     *   --dump-schedule     Print the ECS schedule (phases, stages, and why), then exit.
     *                       Exits with code 1 if the schedule has errors
     *   --validate-schedule Check the ECS schedule, print any errors, then exit with
     *                       code 0 (no errors) or 1. The build runs this (fling_validate_schedule)
     *   --dump-ticks F      Write one line per tick to file F: the tick, its input cut, its
     *                       input counts and the state hash (the tick ledger), to diff two runs
     *   --exec M            How play runs: threaded (ticks on a game thread), direct or inline
     *                       (one thread, frame systems on a published copy). Overrides [Engine] Exec
     *   --sim-hz N          The fixed simulation rate, ticks a second. Overrides [Engine] SimHz
     *   --max-fps N         At most N frames a second on the main thread; 0: no cap. Overrides
     *                       [Engine] MaxFps. RunSettings has the defaults and the valid ranges
     *   --vsync on|off      Present waits for the display's refresh (on) or not (off: frames
     *                       may tear, for the lowest latency). Overrides [Engine] VSync
     *   --interpolation on|off
     *                       Draw between the latest two published ticks (on) or the latest tick
     *                       only. Overrides [Engine] Interpolation
     *   --thread-jitter S:U --exec threaded only: sleep a random 0..U microseconds (seed S) before each wake
     *                       and after each publish on the game thread, and before each frame
     */
    struct LaunchOptions
    {
        bool NullRender = false;

        /** Render into offscreen images: no window, no surface (--offscreen). */
        bool Offscreen = false;

        /** --resolution: the frame or window size. 0 means use the config. */
        uint32 ResolutionWidth = 0;
        uint32 ResolutionHeight = 0;

        /** The PNG file to save rendered frames to (--screenshot). Empty means none. */
        String Screenshot;

        /** The ticks to save a frame at (--screenshot-ticks). Empty: the last tick of the run. Sorted. */
        std::vector<uint64> ScreenshotTicks;

        /** The JSON file to write the simulation state to (--dump-state). Empty means none. */
        String DumpState;

        /** The ticks to write the state at (--dump-state-ticks). Empty: the last tick of the run. Sorted. */
        std::vector<uint64> DumpStateTicks;

        /** True with no window (--null-render, --offscreen): a virtual clock and no frame cap. */
        bool IsHeadless() const { return NullRender || Offscreen; }

        bool Unattended = false;

        /** Number of frames to run before the engine exits (--frames). 0 means no limit. */
        uint32 Frames = 0;

        /** With NullRender: frames per fixed tick (--frames-per-tick). 1 or more. */
        uint32 FramesPerTick = 1;

        /** True when --seed was given. If false, the seed comes from the operating system. */
        bool HasSeed = false;

        uint32 Seed = 0;

        LogFormat Format = LogFormat::Text;

        InputLogLevel InputLog = InputLogLevel::Off;

        /** The level to load at start (--level), relative to the assets folder. Empty means use the config. */
        String Level;

        /** The input file to play (--replay-input). Empty means no replay. */
        String ReplayInput;

        /** The file to save an input recording to (--record-input). Empty means no recording. */
        String RecordInput;

        /** Print the ECS schedule, then exit (--dump-schedule). */
        bool DumpSchedule = false;

        /** Check the ECS schedule, then exit (--validate-schedule). */
        bool ValidateSchedule = false;

        /** The file to write the tick ledger rows to (--dump-ticks). Empty means none. */
        String DumpTicks;

        /** How play runs (--exec). Empty: not on the command line (RunSettings picks it). */
        std::optional<ExecMode> Exec;

        /** The simulation rate (--sim-hz). Empty: not on the command line. RunSettings checks the range. */
        std::optional<uint32> SimHz;

        /** The frame cap (--max-fps), 0 for none. Empty: not on the command line. RunSettings checks the range. */
        std::optional<uint32> MaxFps;

        /** --vsync on or off. Empty: not on the command line. */
        std::optional<bool> VSync;

        /** --interpolation on or off. Empty: not on the command line. */
        std::optional<bool> Interpolation;

        /** --thread-jitter: the seed and the longest sleep in microseconds. 0 microseconds means off. */
        uint32 JitterSeed = 0;
        uint32 JitterMaxMicroseconds = 0;

        /** The language code to show text in (--language), for example "fr". Empty means use the config. */
        String Language;

        /**
         * A CSV file to write every string table entry to (--loc-export), for translators.
         * The engine writes it and exits, with no window.
         */
        String LocExport;

        /**
         * A CSV file of translations to read into the string tables (--loc-import). The
         * engine writes the JSON tables of each language in the file and exits, with no
         * window.
         */
        String LocImport;

        /**
         * A source folder to search for string table entries (--loc-gather). Entries that
         * the source language does not have are added to its tables, with the source text
         * from the code. The engine writes them and exits, with no window.
         */
        String LocGather;

        /** True when a localization flag asks the engine to do its work and exit. */
        bool IsLocalizationTool() const { return !LocExport.empty() || !LocImport.empty() || !LocGather.empty(); }

        /**
         * Read the launch flags from a command line. Flags that are not given keep
         * their default values.
         *
         * @param t_CmdLine     The command line to read
         * @param t_Out         Receives the options
         * @param t_OutError    Receives a message for the first flag that is not valid
         * @return False if a flag has a value that is not valid
         */
        static bool Parse(const CommandLine& t_CmdLine, LaunchOptions& t_Out, String& t_OutError);
    };

    /**
     * The file name of one capture (--screenshot, --dump-state) at a tick. "{tick}" in the path
     * is replaced with the tick. Without it, when there is more than one tick, "_tick<N>" goes
     * before the extension (shot.png: shot_tick30.png). Otherwise the path is used as it is.
     */
    String CaptureFilePath(StringView t_Path, uint64 t_Tick, bool t_ManyTicks);

    /** Read "on" or "off" (a switch setting such as --vsync or [Engine] VSync). False for any other text. */
    inline bool ParseOnOff(const StringView t_Text, bool& t_Out)
    {
        if (t_Text == "on" || t_Text == "off")
        {
            t_Out = t_Text == "on";
            return true;
        }
        return false;
    }
} // namespace Fling
