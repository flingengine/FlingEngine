#include "Misc/SimulationCheck.h"

#include <catch2/catch_all.hpp>
#include <thread>

using Fling::SimulationCheck;

TEST_CASE("SimulationCheck marks the thread while a scope lives", "[simulation]")
{
    REQUIRE_FALSE(SimulationCheck::IsActive());
    {
        const SimulationCheck::Scope outer;
        REQUIRE(SimulationCheck::IsActive());
        {
            const SimulationCheck::Scope inner;
            REQUIRE(SimulationCheck::IsActive());
        }
        // The inner scope puts back the value it found.
        REQUIRE(SimulationCheck::IsActive());

        // The mark is per thread.
        bool otherThreadActive = true;
        std::thread([&] { otherThreadActive = SimulationCheck::IsActive(); }).join();
        REQUIRE_FALSE(otherThreadActive);
    }
    REQUIRE_FALSE(SimulationCheck::IsActive());
}
