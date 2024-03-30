#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_all.hpp>

#include "pch.h"

TEST_CASE("Renderer", "[Renderer]")
{
    SECTION("Smoke test")
    {
        REQUIRE(true);
    }
}