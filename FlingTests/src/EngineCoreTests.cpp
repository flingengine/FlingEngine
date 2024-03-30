//#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file

//#include "catch2/catch.hpp"

#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_all.hpp>

#include "pch.h"

#include "Engine.h"

TEST_CASE("Smoke test", "[core]")
{

    SECTION("No command line options")
    {
        Fling::Engine e = {};

        REQUIRE(true);
    }
 
    SECTION("Test command line flags")
    {
        //int argc = 2;
        //char* argv[] = { "-flag", "--flag2" };
        //Fling::Engine e = { argc, argv };

        REQUIRE(true);
    }

}