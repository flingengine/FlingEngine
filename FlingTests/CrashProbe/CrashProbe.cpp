// Crashes on purpose after CrashHandler::InstallUnattended, so that CTest can
// check that an unattended crash prints a stack trace, exits with
// ExitCode::Crash, and does not open a dialog. See ExpectCrash.cmake.

#include "Misc/CrashHandler.h"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

int main(int argc, const char* argv[]) {
    if (argc != 2) {
        std::fputs("Usage: CrashProbe <abort|assert|segv|throw>\n", stderr);
        return 3;
    }

    Fling::CrashHandler::InstallUnattended();

    const char* Mode = argv[1];
    if (std::strcmp(Mode, "abort") == 0) {
        std::abort();
    } else if (std::strcmp(Mode, "assert") == 0) {
#ifdef NDEBUG
        // assert() is compiled out of release builds. A failed assert calls abort().
        std::abort();
#else
        const bool bAlwaysFalse = argc < 0;
        assert(bAlwaysFalse);
#endif
    } else if (std::strcmp(Mode, "segv") == 0) {
        volatile int* Null = nullptr;
        *Null = 1;
    } else if (std::strcmp(Mode, "throw") == 0) {
        throw std::runtime_error("CrashProbe uncaught exception");
    }

    std::fprintf(stderr, "CrashProbe: unknown mode '%s'\n", Mode);
    return 3;
}
