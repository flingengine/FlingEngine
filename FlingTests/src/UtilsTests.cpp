#include "catch2/catch.hpp"

#include "pch.h"

#include "Engine.h"
#include "Singleton.hpp"
#include "Random.h"
#include "Logger.h"

TEST_CASE("Timing", "[utils]")
{
    SECTION("valid Config")
    {
        using namespace Fling;
        REQUIRE(true);
        Timing::Get().Init();

        // Update timing
        Timing::Get().Update();
        float deltaTime = Timing::Get().GetDeltaTime();
        float totalTime = Timing::Get().GetTimef();
        (void)(deltaTime);
        (void)(totalTime);
    }
}

TEST_CASE("Logger", "[utils]")
{
    SECTION("valid Config")
    {
        REQUIRE(true);
    }
}

TEST_CASE("Random", "[utils]")
{
    REQUIRE_FALSE(Fling::Random::bIsInitalized);

    Fling::Random::Init();

    REQUIRE(Fling::Random::bIsInitalized);
}