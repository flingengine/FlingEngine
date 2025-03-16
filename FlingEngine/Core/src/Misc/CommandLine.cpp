#include "Misc/CommandLine.h"
#include <sstream>

namespace Fling
{
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
		std::stringstream CmdStream;

		// TODO: For each argument, parse it into a map or something for quick lookup later
		// where the key is the string after the "-" and the value is the string before the next " -"

		// Start at 1 to exclude the first argument(the executable name)
		for(int32 i = 1; i < argc; ++i)
		{
			CmdStream << argv[i];

			// Add a space between each arg except for the last one
			if(i + 1 < argc)
			{
				CmdStream << " ";
			}
		}
		CurrentCommandLineData = CmdStream.str();

		// As long as our command line is not empty, we should be fine...
		// TODO: make sure our command line map is the same size as argc
		return !CurrentCommandLineData.empty();
	}

	bool CommandLine::HasParam(const std::string_view Param) const
	{
		std::size_t found = CurrentCommandLineData.find(Param);

		return found != std::string::npos;
	}

	const char* CommandLine::GetValueAsString(const std::string_view Param) const
	{
		// TODO: Regex match for this param and get it's value as a string
		// Some examples of allowed sytanx

		// -myFlag=false
		// -myFlag=1
		// -stringFlag="this is a string flag"
		// -numericValue=-1123.56
		
		// So when you call this function, the "Param" is the key part of the command line argument.
		// Such as "myFlag" or "stringValue" in the examples above. We are searching the command
		// line that we have been given for this value.
		

		return nullptr;
	}

	std::string_view CommandLine::GetCommandLineData() const
	{
		return CurrentCommandLineData;
	}
} // namespace Fling
