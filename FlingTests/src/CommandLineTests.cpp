#define CATCH_CONFIG_MAIN  // This tells Catch to provide a main() - only do this in one cpp file
#define CATCH_CONFIG_ENABLE_BENCHMARKING
#include <catch2/catch_all.hpp>

#include "Misc/CommandLine.h"
#include <string_view>
#include <fstream>
#include <cstdio>

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

	SECTION("Bool Flag - true")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-test=true"
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

		const int32 Value = CommandLine::Get().GetValueAs<int32>("numFlag", -1);
		REQUIRE(Value == 776);

		// Should also be readable as a plain int
		const int PlainIntValue = CommandLine::Get().GetValueAs<int>("numFlag", -1);
		REQUIRE(PlainIntValue == 776);
	}

	SECTION("Double Flag")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-numericValue=-1123.56",
		};

		int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		const double Value = CommandLine::Get().GetValueAs<double>("numericValue", 0.0);
		REQUIRE(Value == Catch::Approx(-1123.56));
	}

	SECTION("Multiple flags must be separate argv entries")
	{
		// A real shell splits "-foo=1 -bar=2" into two argv entries before the program
		// ever sees them. A launcher that instead hands both flags over as a single
		// argv string (e.g. a misconfigured launch.json "args" entry) will not get
		// split back apart here - the second flag's key/value just becomes part of
		// the first flag's value. This test documents that gotcha.
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-BenTest=false -TestDouble=420.69"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		REQUIRE(CommandLine::Get().HasParam("BenTest"));
		REQUIRE_FALSE(CommandLine::Get().HasParam("TestDouble"));

		// Passed as two separate argv entries, both are parsed correctly
		const char* CorrectArgs[] =
		{
			"FlingEngine.exe",
			"-BenTest=false",
			"-TestDouble=420.69"
		};
		constexpr int32 CorrectArgCount = sizeof(CorrectArgs) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(CorrectArgCount, CorrectArgs));

		REQUIRE(CommandLine::Get().GetValueAs<bool>("BenTest", true) == false);
		REQUIRE(CommandLine::Get().GetValueAs<double>("TestDouble", -1.0) == Catch::Approx(420.69));
	}

	SECTION("Bool Flag - implicit true")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-BoolFlag"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		REQUIRE(CommandLine::Get().HasParam("BoolFlag"));
		REQUIRE(CommandLine::Get().GetValueAs<bool>("BoolFlag", false) == true);
	}

	SECTION("Bool Flag - double dash implicit true")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"--BoolFlag"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		REQUIRE(CommandLine::Get().HasParam("BoolFlag"));
		REQUIRE(CommandLine::Get().GetValueAs<bool>("BoolFlag", false) == true);
	}

	SECTION("Double dash key-value flag")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"--resolutionWidth=1920"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		REQUIRE(CommandLine::Get().HasParam("resolutionWidth"));
		REQUIRE(CommandLine::Get().GetValueAs<int32>("resolutionWidth", -1) == 1920);
	}

	SECTION("Argument exactly at the max length is parsed")
	{
		const std::string Prefix = "-longFlag=";
		const std::string ArgAtLimit = Prefix + std::string(CommandLine::MaxArgLength - Prefix.size(), 'a');
		REQUIRE(ArgAtLimit.size() == CommandLine::MaxArgLength);

		const char* Args[] =
		{
			"FlingEngine.exe",
			ArgAtLimit.c_str()
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		REQUIRE(CommandLine::Get().HasParam("longFlag"));
	}

	SECTION("Argument over the max length is ignored")
	{
		const std::string Prefix = "-longFlag=";
		const std::string ArgOverLimit = Prefix + std::string(CommandLine::MaxArgLength - Prefix.size() + 1, 'a');
		REQUIRE(ArgOverLimit.size() == CommandLine::MaxArgLength + 1);

		const char* Args[] =
		{
			"FlingEngine.exe",
			ArgOverLimit.c_str()
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		REQUIRE_FALSE(CommandLine::Get().HasParam("longFlag"));
	}

	SECTION("String Flag - quoted value")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-stringFlag=\"I like this format\""
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		REQUIRE(CommandLine::Get().HasParam("stringFlag"));

		const std::string_view Value = CommandLine::Get().GetValueAsString("stringFlag");
		REQUIRE(Value == "I like this format");
	}

	SECTION("Missing param falls back to default")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-numFlag=776"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		const bool bWasInitalized = CommandLine::Get().Init(ArgCount, Args);
		REQUIRE(bWasInitalized);

		REQUIRE_FALSE(CommandLine::Get().HasParam("notPresent"));
		REQUIRE(CommandLine::Get().GetValueAsString("notPresent") == nullptr);
		REQUIRE(CommandLine::Get().GetValueAs<int32>("notPresent", 42) == 42);
	}

	SECTION("Console Variables - loaded from ini string")
	{
		const char* Args[] =
		{
			"FlingEngine.exe"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		const std::string IniContent =
			"[Video]\n"
			"resolutionWidth=1280\n"
			"\n"
			"[ConsoleVariables]\n"
			"resolutionWidth=1920\n"
			"fullscreen=true\n"
			"windowTitle=\"Fling Engine\"\n";

		REQUIRE(CommandLine::Get().LoadConfigVarsFromString(IniContent));

		// Values from the [ConsoleVariables] section should be picked up as if
		// they were passed on the command line
		REQUIRE(CommandLine::Get().HasParam("resolutionWidth"));
		REQUIRE(CommandLine::Get().GetValueAs<int32>("resolutionWidth", -1) == 1920);
		REQUIRE(CommandLine::Get().GetValueAs<bool>("fullscreen", false) == true);

		const std::string_view Title = CommandLine::Get().GetValueAsString("windowTitle");
		REQUIRE(Title == "Fling Engine");

		// Keys from other sections should not be picked up
		REQUIRE_FALSE(CommandLine::Get().HasParam("nonConsoleVariableKey"));
	}

	SECTION("Console Variables - command line takes precedence over ini file")
	{
		const char* Args[] =
		{
			"FlingEngine.exe",
			"-windowTitle=FromCommandLine"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		const std::string IniContent =
			"[ConsoleVariables]\n"
			"windowTitle=FromConfigFile\n"
			"onlyInConfig=42\n";

		REQUIRE(CommandLine::Get().LoadConfigVarsFromString(IniContent));

		// The command line explicitly set windowTitle, so it should win over the ini value
		const std::string_view Title = CommandLine::Get().GetValueAsString("windowTitle");
		REQUIRE(Title == "FromCommandLine");

		// Values only present in the ini file should still be picked up
		REQUIRE(CommandLine::Get().HasParam("onlyInConfig"));
		REQUIRE(CommandLine::Get().GetValueAs<int32>("onlyInConfig", -1) == 42);
	}

	SECTION("Console Variables - loaded from an actual ini file on disk")
	{
		const char* Args[] =
		{
			"FlingEngine.exe"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		const std::string TempFilePath = "CommandLineTests_Temp.ini";
		{
			std::ofstream OutFile(TempFilePath);
			OutFile << "[ConsoleVariables]\n";
			OutFile << "numEnemies=17\n";
		}

		REQUIRE(CommandLine::Get().LoadConfigFile(TempFilePath));
		REQUIRE(CommandLine::Get().GetValueAs<int32>("numEnemies", -1) == 17);

		std::remove(TempFilePath.c_str());
	}

	SECTION("Console Variables - missing file fails to load")
	{
		const char* Args[] =
		{
			"FlingEngine.exe"
		};
		constexpr int32 ArgCount = sizeof(Args) / sizeof(char*);
		REQUIRE(CommandLine::Get().Init(ArgCount, Args));

		REQUIRE_FALSE(CommandLine::Get().LoadConfigFile("ThisFileDoesNotExist.ini"));
	}
}