#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_all.hpp>

#include "pch.h"

#include "Engine.h"
#include "Singleton.hpp"
#include "Random.h"
#include "Logger.h"
#include "FreeList.h"
#include "StackAllocator.h"
#include "Memory.h"
#include "CircularBuffer.hpp"

#include <vector>

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
    using namespace Fling;

    SECTION("Init sets the initialized flag")
    {
        REQUIRE_FALSE(Random::bIsInitalized);

        Random::Init();

        REQUIRE(Random::bIsInitalized);
    }

    SECTION("The same seed reproduces the same sequence")
    {
        Random::Init(1234u);
        std::vector<int> first;
        for (int i = 0; i < 20; ++i)
        {
            first.push_back(Random::RandomBetween(0, 1000));
        }

        Random::Init(1234u);
        std::vector<int> second;
        for (int i = 0; i < 20; ++i)
        {
            second.push_back(Random::RandomBetween(0, 1000));
        }

        REQUIRE(first == second);
    }

    SECTION("Different seeds produce different sequences")
    {
        Random::Init(1u);
        std::vector<int> first;
        for (int i = 0; i < 20; ++i)
        {
            first.push_back(Random::RandomBetween(0, 1000000));
        }

        Random::Init(2u);
        std::vector<int> second;
        for (int i = 0; i < 20; ++i)
        {
            second.push_back(Random::RandomBetween(0, 1000000));
        }

        REQUIRE(first != second);
    }

    SECTION("Random0ToN stays within [0, max)")
    {
        Random::Init(42u);
        for (int i = 0; i < 1000; ++i)
        {
            const int value = Random::Random0ToN(10);
            REQUIRE(value >= 0);
            REQUIRE(value < 10);
        }
    }

    SECTION("RandomBetween stays within [min, max]")
    {
        Random::Init(42u);
        for (int i = 0; i < 1000; ++i)
        {
            const int value = Random::RandomBetween(5, 15);
            REQUIRE(value >= 5);
            REQUIRE(value <= 15);
        }
    }

    SECTION("GetRandomFloat stays within [min, max]")
    {
        Random::Init(42u);
        for (int i = 0; i < 1000; ++i)
        {
            const float value = Random::GetRandomFloat(-1.0f, 1.0f);
            REQUIRE(value >= -1.0f);
            REQUIRE(value <= 1.0f);
        }
    }

    SECTION("GetRandomVec3 stays within component-wise bounds")
    {
        Random::Init(42u);
        const glm::vec3 min(-1.0f, 0.0f, 2.0f);
        const glm::vec3 max(1.0f, 5.0f, 3.0f);
        for (int i = 0; i < 100; ++i)
        {
            const glm::vec3 value = Random::GetRandomVec3(min, max);
            REQUIRE(value.x >= min.x);
            REQUIRE(value.x <= max.x);
            REQUIRE(value.y >= min.y);
            REQUIRE(value.y <= max.y);
            REQUIRE(value.z >= min.z);
            REQUIRE(value.z <= max.z);
        }
    }
}

TEST_CASE("Logger", "[utils]")
{
    using namespace Fling;
    Logger::Get().Init();


	SECTION("Logger Console Creation")
	{		
		// Require the current console to exist
		REQUIRE(Logger::GetCurrentConsole() != nullptr);
	}

    SECTION("Logger File Creation")
    {
		REQUIRE(Logger::GetCurrentLogFile() != nullptr);
    }
}

TEST_CASE("Free List", "[utils]")
{
	using namespace Fling;

    char buf[1024] = {};

    FreeList freelist(
        /* start = */ buf,
        /* end = */buf + 1024,
        /* elm size */ 32, 
        /* alignment */ 8,
        /* offset */ 0);

    void* obj0 = freelist.Obtain();
	REQUIRE(obj0 != nullptr);

    void* obj1 = freelist.Obtain();	
	REQUIRE(obj1 != nullptr);

	freelist.Return(obj1);
	freelist.Return(obj0);
}

TEST_CASE("Stack Allocator", "[utils]")
{
    using namespace Fling;

    char buf[1024] = {};

    StackAllocator stackAllocator(buf, buf + 1024);
    // @TODO The stack allocator is broken and it seems to be rooted in 
    // the AlignPointer method on linux. That's a relatively large problem
}

TEST_CASE("Aligned Alloc", "[utils]")
{
    void* a = nullptr;
    a = Fling::AlignedAlloc(8, 8);

    REQUIRE(a != nullptr);

    Fling::AlignedFree(a);
}

TEST_CASE("Circular Buffer", "[utils]")
{
    // Circular buffer of char's 
    Fling::CircularBuffer<int32, 128> CircBuf {};

}