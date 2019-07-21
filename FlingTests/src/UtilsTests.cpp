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
        Timing::Get().Init();

        Timing::Get().Update();
        float deltaTime = Timing::Get().GetDeltaTime();
        float totalTime = Timing::Get().GetTimef();

		REQUIRE(totalTime >= 0.0f);
		REQUIRE(deltaTime >= 0.0f);
    }
}

TEST_CASE("Random", "[utils]")
{
    REQUIRE_FALSE(Fling::Random::bIsInitalized);

    Fling::Random::Init();

    REQUIRE(Fling::Random::bIsInitalized);
}

TEST_CASE("Logger", "[utils]")
{
	SECTION("Logger Creation")
	{
		using namespace Fling;

		//Logger::Get().Init();

		// Require the current console to exist
		REQUIRE(Logger::GetCurrentConsole() != nullptr);

		//Logger::Get().Shutdown();
	}
}