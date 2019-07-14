#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file

#include "catch2/catch.hpp"

#include "pch.h"

#include "Engine.h"

TEST_CASE("Smoke test", "[core]")
{

    SECTION("No options")
    {
        //Fling::Engine e = {};

        REQUIRE(true);
    }
    
}