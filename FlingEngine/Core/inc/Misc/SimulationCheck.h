#pragma once

#include "CoreAPI.h"

/**
 * FLING_SIMULATION_CHECKS is 1 in builds with asserts (no NDEBUG). Then
 * FLING_SIMULATION_FORBID(what) reports a call from simulation code. In other builds the
 * macro is empty and does not evaluate its argument.
 */
#if !defined(NDEBUG)
#define FLING_SIMULATION_CHECKS 1
#else
#define FLING_SIMULATION_CHECKS 0
#endif

namespace Fling
{
    /**
     * A debug check for code that the simulation must not call. The ECS runner marks the
     * thread while it runs fixed rate systems. Functions that read the wall clock, the
     * global random stream or the per-frame input (Timing, PlatformTime, Random, the key
     * queries of Input) call FLING_SIMULATION_FORBID, because a simulation that uses them
     * is not the same on every run or every machine. Use SimTime, a per-system random
     * stream and the InputState singleton instead.
     */
    class CORE_API SimulationCheck
    {
    public:
        /** True while this thread runs fixed rate ECS systems. */
        static bool IsActive();

        /** Called on a violation, with what was called and what to use. The default logs an error and asserts. */
        using ViolationHandler = void (*)(const char* what);

        /** Replace the handler (tests count violations). Null restores the default. Returns the old one. */
        static ViolationHandler SetViolationHandler(ViolationHandler handler);

        /** Calls the handler if this thread runs the simulation now. */
        static void Forbid(const char* what)
        {
            if (IsActive()) [[unlikely]]
            {
                ReportViolation(what);
            }
        }

        /** Marks this thread as running the simulation until the scope ends. */
        class CORE_API Scope
        {
        public:
            Scope();
            ~Scope();

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            bool m_WasActive = false;
        };

    private:
        static void ReportViolation(const char* what);
    };
} // namespace Fling

#if FLING_SIMULATION_CHECKS
#define FLING_SIMULATION_FORBID(what) ::Fling::SimulationCheck::Forbid(what)
#else
#define FLING_SIMULATION_FORBID(what) ((void)0)
#endif
