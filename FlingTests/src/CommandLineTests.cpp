#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_all.hpp>

#include "Misc/CommandLine.h"

TEST_CASE("Command Line", "[Command Line]")
{
	using namespace Fling;

	SECTION("Ignores First Argument")
	{
		const char* Args[] = 
		{
			"FlingEngine.exe",
		};

		const int32 ArgCount = sizeof(Args) / sizeof(char*);
		CommandLine::Set(CommandLine::BuildFromArgs(ArgCount, Args));

		// The command line should always ignore the first argument, which is
		// the application name
		const std::string& CurCmdLine = CommandLine::Get();
		REQUIRE(CurCmdLine.length() == 0);
	}

	SECTION("Bool Flag")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-test=918"
		};
		int32 ArgCount = sizeof(Args) / sizeof(char*);
		CommandLine::Set(CommandLine::BuildFromArgs(ArgCount, Args));

		const bool bHasFlag = CommandLine::HasParam("test");
		REQUIRE(bHasFlag);
	}
}