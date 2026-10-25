#pragma once

#include "FlingTypes.h"
#include <string>

namespace Fling {
    class CommandLine;

    /** The format of log lines on the console and in the log file. */
    enum class LogFormat : uint8 {
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
     */
    struct LaunchOptions {
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

        /**
         * Read the launch flags from a command line. Flags that are not given keep
         * their default values.
         *
         * @param t_CmdLine     The command line to read
         * @param t_Out         Receives the options
         * @param t_OutError    Receives a message for the first flag that is not valid
         * @return False if a flag has a value that is not valid
         */
        static bool Parse(const CommandLine& t_CmdLine, LaunchOptions& t_Out, std::string& t_OutError);
    };
} // namespace Fling
