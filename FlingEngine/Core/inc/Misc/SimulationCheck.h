#pragma once

#include "CoreAPI.h"

namespace Fling
{
    /**
     * A debug check for code that the simulation must not call. The ECS runner
     * marks the thread while it runs fixed rate systems. Functions that read the
     * wall clock or the global random stream (Timing, Random) assert that the mark
     * is not set, because a simulation that uses them is not the same on every
     * run or every machine. Use SimTime and a per-system random stream instead.
     */
    class CORE_API SimulationCheck
    {
    public:
        /** True while this thread runs fixed rate ECS systems. */
        static bool IsActive();

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
    };
} // namespace Fling
