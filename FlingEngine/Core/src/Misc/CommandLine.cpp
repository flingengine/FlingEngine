#include "Misc/CommandLine.h"
#include "Strings/String.h"

#include <fstream>
#include <sstream>

namespace Fling
{
	namespace
	{
		/** The ini section that command line style console variables are read from, UE-style. */
		const String ConsoleVariablesSectionName = "ConsoleVariables";

		/** Strips a single pair of surrounding double quotes from a value, e.g. "\"foo bar\"" -> "foo bar" */
		String StripSurroundingQuotes(const StringView Value)
		{
			if (Value.size() >= 2 && Value.front() == '"' && Value.back() == '"')
			{
				return String(Value.substr(1, Value.size() - 2));
			}

			return String(Value);
		}

		/** Strips leading/trailing whitespace (spaces and tabs) from a string */
		String Trim(const StringView Value)
		{
			const std::size_t First = Value.find_first_not_of(" \t");
			if (First == String::npos)
			{
				return "";
			}

			const std::size_t Last = Value.find_last_not_of(" \t");
			return String(Value.substr(First, Last - First + 1));
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
			String Arg = argv[i];

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
				Arg.erase(0, FirstNonDash == String::npos ? Arg.size() : FirstNonDash);
			}

			if (Arg.empty())
			{
				continue;
			}

			const std::size_t EqualsPos = Arg.find('=');
			if (EqualsPos != String::npos)
			{
				const String Key = Arg.substr(0, EqualsPos);
				const String Value = StripSurroundingQuotes(Arg.substr(EqualsPos + 1));
				ParsedArgs[Key] = Value;
			}
			else if (FirstNonDash == 2 && i + 1 < argc && argv[i + 1][0] != '\0' && argv[i + 1][0] != '-'
					 && StringView(argv[i + 1]).size() <= MaxArgLength)
			{
				// "--frames 300": a double dash key with no "=" takes the next arg as
				// its value, if that arg is not a flag itself
				++i;
				CmdStream << argv[i];
				if (i + 1 < argc)
				{
					CmdStream << " ";
				}
				ParsedArgs[Arg] = StripSurroundingQuotes(argv[i]);
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

	bool CommandLine::HasParam(const StringView Param) const
	{
		return FindValue(Param) != nullptr;
	}

	std::optional<StringView> CommandLine::GetValueAsString(const StringView Param) const
	{
		// Some examples of allowed syntax:
		// -myFlag=false
		// -myFlag=1
		// -stringFlag="this is a string flag"
		// -numericValue=-1123.56
		// -BoolFlag              (implicitly "true")

		const String* Value = FindValue(Param);
		return Value != nullptr ? std::optional<StringView>(*Value) : std::nullopt;
	}

	StringView CommandLine::GetCommandLineData() const
	{
		return CurrentCommandLineData;
	}

	const String* CommandLine::FindValue(const StringView Param) const
	{
		const String Key(Param);

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

	bool CommandLine::LoadConfigFile(const StringView FilePath)
	{
		std::ifstream File(PathFromUtf8(FilePath));
		if (!File.is_open())
		{
			return false;
		}

		std::stringstream Buffer;
		Buffer << File.rdbuf();

		return LoadConfigVarsFromString(Buffer.str());
	}

	bool CommandLine::LoadConfigVarsFromString(const StringView IniContent)
	{
		ConfigArgs.clear();

		String CurrentSection;
		std::istringstream Stream{ String(IniContent) };
		String Line;

		while (std::getline(Stream, Line))
		{
			// Trim a trailing carriage return in case this ini uses CRLF line endings
			if (!Line.empty() && Line.back() == '\r')
			{
				Line.pop_back();
			}

			const String Trimmed = Trim(Line);
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
			if (EqualsPos == String::npos)
			{
				continue;
			}

			const String Key = Trim(Trimmed.substr(0, EqualsPos));
			const String Value = StripSurroundingQuotes(Trim(Trimmed.substr(EqualsPos + 1)));

			if (!Key.empty())
			{
				ConfigArgs[Key] = Value;
			}
		}

		return true;
	}
} // namespace Fling
