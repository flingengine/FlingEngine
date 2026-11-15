#pragma once

#include "Input/Input.h"
#include "Misc/SimulationCheck.h"
#include "Threading/Lane.h"

#include <atomic>

// Scopes that tests share: the Input singleton, and counters that take the place of the
// lane and simulation check asserts for the life of the scope.

namespace TestScopes
{
    /** Starts the Input singleton, and always shuts it down and clears its writer lane at the end of the test. */
    struct InputScope
    {
        InputScope() { Fling::Input::Get().Init(); }

        ~InputScope()
        {
            Fling::Input::Get().Shutdown();
            Fling::Input::Get().SetWriterLane(Fling::Lane::None);
        }
    };

    /** Counts lane violations instead of asserting, while alive. Count starts at 0. */
    struct CountLaneViolations
    {
        static inline std::atomic<int> Count{0};

        CountLaneViolations()
        {
            Count = 0;
            m_Previous = Fling::LaneCheck::SetViolationHandler([](Fling::Lane, Fling::Lane) { ++Count; });
        }

        ~CountLaneViolations() { Fling::LaneCheck::SetViolationHandler(m_Previous); }

        CountLaneViolations(const CountLaneViolations&) = delete;
        CountLaneViolations& operator=(const CountLaneViolations&) = delete;

        Fling::LaneCheck::ViolationHandler m_Previous = nullptr;
    };

    /** Counts simulation check violations instead of asserting, while alive. Count starts at 0. */
    struct CountSimulationViolations
    {
        static inline std::atomic<int> Count{0};

        CountSimulationViolations()
        {
            Count = 0;
            m_Previous = Fling::SimulationCheck::SetViolationHandler([](const char*) { ++Count; });
        }

        ~CountSimulationViolations() { Fling::SimulationCheck::SetViolationHandler(m_Previous); }

        CountSimulationViolations(const CountSimulationViolations&) = delete;
        CountSimulationViolations& operator=(const CountSimulationViolations&) = delete;

        Fling::SimulationCheck::ViolationHandler m_Previous = nullptr;
    };
} // namespace TestScopes
