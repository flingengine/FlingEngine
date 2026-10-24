#include <catch2/catch_all.hpp>

#include "Engine.h"

TEST_CASE("Smoke test", "[core]") {
    SECTION("No command line options") {
        Fling::Engine e = {};

        REQUIRE(true);
    }

    SECTION("Test command line flags") {
        // int argc = 2;
        // char* argv[] = { "-flag", "--flag2" };
        // Fling::Engine e = { argc, argv };

        REQUIRE(true);
    }
}
