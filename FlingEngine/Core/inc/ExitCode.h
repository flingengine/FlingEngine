#pragma once

namespace Fling {
    /**
     * Process exit codes of a Fling application. Scripts and CI read these
     * values, so do not change the numbers.
     */
    enum class ExitCode : int {
        /** The run completed with no errors. */
        Success = 0,

        /** A scenario or an assertion failed. */
        Failure = 1,

        /** The process crashed, or a fatal error stopped it. */
        Crash = 2,

        /** The launch arguments or the content are not valid. */
        BadArguments = 3,
    };

    /** The exit code as an int, for main() to return. */
    constexpr int ToInt(ExitCode t_Code) {
        return static_cast<int>(t_Code);
    }
} // namespace Fling
