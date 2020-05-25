#include "catch2/catch.hpp"

#include "pch.h"

#include "Engine.h"
#include "Singleton.hpp"
#include "Random.h"
#include "Logger.h"
#include "FreeList.h"
#include "StackAllocator.h"
#include "Memory.h"
#include "CircularBuffer.hpp"

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
    Fling::CircularBuffer<INT32, 128> CircBuf {};

}