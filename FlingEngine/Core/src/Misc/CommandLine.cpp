#include "Misc/CommandLine.h"
#include <sstream>
#include <fstream>

namespace Fling
{
	namespace
	{
		/** The ini section that command line style console variables are read from, UE-style. */
		const std::string ConsoleVariablesSectionName = "ConsoleVariables";

		/** Strips a single pair of surrounding double quotes from a value, e.g. "\"foo bar\"" -> "foo bar" */
		std::string StripSurroundingQuotes(const std::string& Value)
		{
			if (Value.size() >= 2 && Value.front() == '"' && Value.back() == '"')
			{
				return Value.substr(1, Value.size() - 2);
			}

			return Value;
		}

		/** Strips leading/trailing whitespace (spaces and tabs) from a string */
		std::string Trim(const std::string& Value)
		{
			const std::size_t First = Value.find_first_not_of(" \t");
			if (First == std::string::npos)
			{
				return "";
			}

			const std::size_t Last = Value.find_last_not_of(" \t");
			return Value.substr(First, Last - First + 1);
		}
	}

	CommandLine& CommandLine::Get()
	{
		// the command line is a singleton... you can only pass
		// in one command line instance for the application's lifetime
		
		// TODO: Maybe make this a singleton on the FlingEngine type, not
		// on it's own, so that you can technically run multiple instances
		// of the engine like if you have multi-window game previews?
		static Fling::CommandLine Instance = {};

		return Instance;
	}

	bool CommandLine::Init(const int32 argc, const char* argv[])
	{
		if (argc <= 0 || argv == nullptr)
		{
			return false;
		}

		ParsedArgs.clear();
		ConfigArgs.clear();

		std::stringstream CmdStream;

		// Start at 1 to exclude the first argument(the executable name)
		for(int32 i = 1; i < argc; ++i)
		{
			std::string Arg = argv[i];

			CmdStream << Arg;

			// Add a space between each arg except for the last one
			if(i + 1 < argc)
			{
				CmdStream << " ";
			}

			// CommandLine::Init runs very early in Engine startup, before logging is
			// guaranteed to be available, so oversized arguments are ignored silently
			// rather than logged.
			if (Arg.size() > MaxArgLength)
			{
				continue;
			}

			// Args are expected to be prefixed with one or more "-", e.g. "-foo=7" or "--foo=7"
			const std::size_t FirstNonDash = Arg.find_first_not_of('-');
			if (FirstNonDash != 0)
			{
				Arg.erase(0, FirstNonDash == std::string::npos ? Arg.size() : FirstNonDash);
			}

			if (Arg.empty())
			{
				continue;
			}

			const std::size_t EqualsPos = Arg.find('=');
			if (EqualsPos != std::string::npos)
			{
				const std::string Key = Arg.substr(0, EqualsPos);
				const std::string Value = StripSurroundingQuotes(Arg.substr(EqualsPos + 1));
				ParsedArgs[Key] = Value;
			}
			else
			{
				// A flag with no explicit value, e.g. "-BoolFlag", is treated as true
				ParsedArgs[Arg] = "true";
			}
		}
		CurrentCommandLineData = CmdStream.str();

		return true;
	}

	bool CommandLine::HasParam(const std::string_view Param) const
	{
		return FindValue(Param) != nullptr;
	}

	const char* CommandLine::GetValueAsString(const std::string_view Param) const
	{
		// Some examples of allowed syntax:
		// -myFlag=false
		// -myFlag=1
		// -stringFlag="this is a string flag"
		// -numericValue=-1123.56
		// -BoolFlag              (implicitly "true")

		const std::string* Value = FindValue(Param);
		return Value != nullptr ? Value->c_str() : nullptr;
	}

	std::string_view CommandLine::GetCommandLineData() const
	{
		return CurrentCommandLineData;
	}

	const std::string* CommandLine::FindValue(const std::string_view Param) const
	{
		const std::string Key(Param);

		// Values passed directly on the command line always win over ones loaded from a config file
		auto Iter = ParsedArgs.find(Key);
		if (Iter != ParsedArgs.end())
		{
			return &Iter->second;
		}

		Iter = ConfigArgs.find(Key);
		if (Iter != ConfigArgs.end())
		{
			return &Iter->second;
		}

		return nullptr;
	}

	bool CommandLine::LoadConfigFile(const std::string& FilePath)
	{
		std::ifstream File(FilePath);
		if (!File.is_open())
		{
			return false;
		}

		std::stringstream Buffer;
		Buffer << File.rdbuf();

		return LoadConfigVarsFromString(Buffer.str());
	}

	bool CommandLine::LoadConfigVarsFromString(const std::string_view IniContent)
	{
		ConfigArgs.clear();

		std::string CurrentSection;
		std::istringstream Stream{ std::string(IniContent) };
		std::string Line;

		while (std::getline(Stream, Line))
		{
			// Trim a trailing carriage return in case this ini uses CRLF line endings
			if (!Line.empty() && Line.back() == '\r')
			{
				Line.pop_back();
			}

			const std::string Trimmed = Trim(Line);
			if (Trimmed.empty() || Trimmed.front() == ';' || Trimmed.front() == '#')
			{
				// Blank line or comment
				continue;
			}

			if (Trimmed.front() == '[' && Trimmed.back() == ']')
			{
				CurrentSection = Trimmed.substr(1, Trimmed.size() - 2);
				continue;
			}

			if (CurrentSection != ConsoleVariablesSectionName)
			{
				// Only console variables are pulled into the command line lookup
				continue;
			}

			const std::size_t EqualsPos = Trimmed.find('=');
			if (EqualsPos == std::string::npos)
			{
				continue;
			}

			const std::string Key = Trim(Trimmed.substr(0, EqualsPos));
			const std::string Value = StripSurroundingQuotes(Trim(Trimmed.substr(EqualsPos + 1)));

			if (!Key.empty())
			{
				ConfigArgs[Key] = Value;
			}
		}

		return true;
	}
} // namespace Fling
