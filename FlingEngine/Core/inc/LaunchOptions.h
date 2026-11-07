#pragma once

#include "FlingTypes.h"
#include "Input/InputMessage.h"
#include "Strings/String.h"

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
     *   --unattended        No modal dialogs. A crash prints a stack trace and exits with code 2
     *   --frames N          Run N ticks, then exit
     *   --seed S            Seed the random number generator
     *   --log-format json   Write logs as JSON lines
     *   --level F           Load level file F (relative to Assets/) instead of [Game] StartLevel
     *   --log-input [all]   Log each input message. "all" also logs mouse movement.
     *                       Device connects and disconnects are always logged
     *   --replay-input F    Play the input messages in file F from the first tick
     *   --record-input F    Record the input messages, and save them to file F at exit
     *                       (--replay-input and --record-input are errors in shipping builds)
     */
    struct LaunchOptions
    {
        /** The tick rate of a --null-render run, in ticks per second. */
        static constexpr uint32 NullRenderTickRate = 60;

        bool NullRender = false;

        bool Unattended = false;

        /** Number of ticks to run before the engine exits. 0 means no limit. */
        uint32 Frames = 0;

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
} // namespace Fling
