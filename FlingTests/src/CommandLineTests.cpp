#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_all.hpp>

#include "Misc/CommandLine.h"
#include <string_view>

TEST_CASE("Command Line", "[Command Line]")
{
	using namespace Fling;

	SECTION("Ignores First Argument")
	{
		const char* Args[] = 
		{
			"FlingEngine.exe",
		};

		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		// The command line should always ignore the first argument, which is
		// the application name
		const std::string_view CurCmdLine = CommandLine::Get().GetCommandLineData();
		REQUIRE(CurCmdLine.empty());
	}

	SECTION("Bool Flag")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-test=918"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		const bool bHasFlag = CommandLine::Get().HasParam("test");
		REQUIRE(bHasFlag);
	}

	SECTION("Integer Flag")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-numFlag=776",
		};

		// Set the command line
		int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		const bool bHasFlag = CommandLine::Get().HasParam("numFlag");
		REQUIRE(bHasFlag);
	}
}