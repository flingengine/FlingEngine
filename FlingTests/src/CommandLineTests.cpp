#include "catch2/catch.hpp"
#include "Misc/CommandLine.h"

TEST_CASE("Command Line", "[Command Line]")
{
	using namespace Fling;

	SECTION("Ignores First Argument")
	{
		char* Args[] = 
		{
			"FlingEngine.exe",
		};
		int32 ArgCount = sizeof(Args) / sizeof(char*);
		CommandLine::Set(CommandLine::BuildFromArgs(ArgCount, Args));

		// The command line should always ignore the first argument, which is
		// the application name
		const std::string& CurCmdLine = CommandLine::Get();
		REQUIRE(CurCmdLine.length() == 0);
	}

	SECTION("Bool Flag")
	{
		char* Args[] =
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