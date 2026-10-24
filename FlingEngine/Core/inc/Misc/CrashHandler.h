#pragma once

#include <cstdio>

namespace Fling {
    /**
     * Crash handling for runs with no human present (CI, agents, --unattended).
     */
    namespace CrashHandler {
        /**
         * Stop asserts, aborts, and crashes from opening a modal dialog. After this
         * call, a crash, a failed assert, or an uncaught exception writes a message
         * and a stack trace to stderr, then exits the process with ExitCode::Crash.
         *
         * Call this once, as early as possible in startup.
         */
        void InstallUnattended();

        /**
         * Write the stack of the calling thread to t_File. It is safe to call this
         * from a crash handler, but symbol names can be missing.
         */
        void PrintStackTrace(std::FILE* t_File);
    } // namespace CrashHandler
} // namespace Fling
