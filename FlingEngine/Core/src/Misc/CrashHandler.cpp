#include "Misc/CrashHandler.h"

// Configured out with WITH_CRASH_HANDLER_FLAG=OFF: the header then has empty inline functions
#if WITH_CRASH_HANDLER

#include "ExitCode.h"
#include "Platform.h"

#include <csignal>
#include <cstdlib>
#include <exception>

#if FLING_WINDOWS
#include <crtdbg.h>
#include <dbghelp.h>
#elif __has_include(<execinfo.h>)
#include <execinfo.h>
#include <unistd.h>
#define FLING_HAS_EXECINFO 1
#endif

namespace Fling {
    namespace {
        constexpr int MaxStackFrames = 64;

        [[noreturn]] void ExitAfterCrash() {
            CrashHandler::PrintStackTrace(stderr);
            std::fflush(stderr);
            std::fflush(stdout);
            std::_Exit(ToInt(ExitCode::Crash));
        }

        const char* SignalName(int t_Signal) {
            switch (t_Signal) {
            case SIGABRT:
                return "SIGABRT (abort or failed assert)";
            case SIGSEGV:
                return "SIGSEGV (bad memory access)";
            case SIGFPE:
                return "SIGFPE (arithmetic error)";
            case SIGILL:
                return "SIGILL (illegal instruction)";
#ifdef SIGBUS
            case SIGBUS:
                return "SIGBUS (bus error)";
#endif
            default:
                return "unknown signal";
            }
        }

        void OnFatalSignal(int t_Signal) {
            std::fprintf(stderr, "\nFling: fatal signal %d: %s\n", t_Signal, SignalName(t_Signal));
            ExitAfterCrash();
        }

        void OnTerminate() {
            std::fputs("\nFling: fatal error: std::terminate was called", stderr);

            if (std::exception_ptr Current = std::current_exception()) {
                try {
                    std::rethrow_exception(Current);
                } catch (const std::exception& e) {
                    std::fprintf(stderr, " by an uncaught exception: %s", e.what());
                } catch (...) {
                    std::fputs(" by an uncaught exception of an unknown type", stderr);
                }
            }

            std::fputs("\n", stderr);
            ExitAfterCrash();
        }

#if FLING_WINDOWS
        LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* t_Info) {
            const DWORD Code = t_Info && t_Info->ExceptionRecord ? t_Info->ExceptionRecord->ExceptionCode : 0;
            std::fprintf(stderr, "\nFling: fatal unhandled exception 0x%08lX\n", static_cast<unsigned long>(Code));
            ExitAfterCrash();
        }
#endif
    } // namespace

    void CrashHandler::InstallUnattended() {
#if FLING_WINDOWS
        // No Windows Error Reporting dialog, and no "file not found" dialogs
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);

        // assert() writes to stderr and does not open a message box
        _set_error_mode(_OUT_TO_STDERR);

#if defined(_MSC_VER)
        // abort() does not show its message box and does not call Windows Error Reporting
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

        // The debug CRT writes its reports to stderr, not to a dialog. These are
        // no-ops in a release CRT.
        _CrtSetReportMode(_CRT_WARN, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_WARN, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
        _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
        _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif

        SetUnhandledExceptionFilter(OnUnhandledException);
#else
        std::signal(SIGSEGV, OnFatalSignal);
        std::signal(SIGFPE, OnFatalSignal);
        std::signal(SIGILL, OnFatalSignal);
#ifdef SIGBUS
        std::signal(SIGBUS, OnFatalSignal);
#endif
#endif

        std::signal(SIGABRT, OnFatalSignal);
        std::set_terminate(OnTerminate);
    }

    void CrashHandler::PrintStackTrace(std::FILE* t_File) {
        std::fputs("Stack trace:\n", t_File);

#if FLING_WINDOWS
        void* Frames[MaxStackFrames] = {};
        const USHORT FrameCount = CaptureStackBackTrace(0, MaxStackFrames, Frames, nullptr);

        HANDLE Process = GetCurrentProcess();
        static const bool bSymbolsReady = SymInitialize(Process, nullptr, TRUE) == TRUE;

        alignas(SYMBOL_INFO) char SymbolStorage[sizeof(SYMBOL_INFO) + 256] = {};
        SYMBOL_INFO* Symbol = reinterpret_cast<SYMBOL_INFO*>(SymbolStorage);
        Symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
        Symbol->MaxNameLen = 255;

        for (USHORT i = 0; i < FrameCount; ++i) {
            const DWORD64 Address = reinterpret_cast<DWORD64>(Frames[i]);
            DWORD64 Displacement = 0;
            if (bSymbolsReady && SymFromAddr(Process, Address, &Displacement, Symbol)) {
                IMAGEHLP_LINE64 Line = {};
                Line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
                DWORD LineDisplacement = 0;
                if (SymGetLineFromAddr64(Process, Address, &LineDisplacement, &Line)) {
                    std::fprintf(t_File,
                                 "  #%u %s (%s:%lu)\n",
                                 i,
                                 Symbol->Name,
                                 Line.FileName,
                                 static_cast<unsigned long>(Line.LineNumber));
                } else {
                    std::fprintf(
                        t_File, "  #%u %s + 0x%llX\n", i, Symbol->Name, static_cast<unsigned long long>(Displacement));
                }
            } else {
                std::fprintf(t_File, "  #%u 0x%llX\n", i, static_cast<unsigned long long>(Address));
            }
        }
#elif defined(FLING_HAS_EXECINFO)
        void* Frames[MaxStackFrames] = {};
        const int FrameCount = backtrace(Frames, MaxStackFrames);
        std::fflush(t_File);
        backtrace_symbols_fd(Frames, FrameCount, fileno(t_File));
#else
        std::fputs("  (stack traces are not available on this platform)\n", t_File);
#endif

        std::fflush(t_File);
    }
} // namespace Fling

#endif // WITH_CRASH_HANDLER
