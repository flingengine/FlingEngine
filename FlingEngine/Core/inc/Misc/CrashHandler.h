#pragma once

#include <cstdio>

namespace Fling {
    /**
     * Crash handling for runs with no human present (CI, agents, --unattended).
     *
     * There is no cost until a crash: InstallUnattended runs once at startup, and
     * PrintStackTrace runs only while the process crashes. Configure with
     * WITH_CRASH_HANDLER_FLAG=OFF to compile it out (for example, when a shipping
     * title has its own crash reporter). The functions then do nothing.
     */
    namespace CrashHandler {
#if WITH_CRASH_HANDLER
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
#else
        inline void InstallUnattended() {}

        inline void PrintStackTrace(std::FILE*) {}
#endif

        /** True when the crash handler is compiled in (WITH_CRASH_HANDLER). */
        constexpr bool IsAvailable() {
            return WITH_CRASH_HANDLER != 0;
        }
    } // namespace CrashHandler
} // namespace Fling
