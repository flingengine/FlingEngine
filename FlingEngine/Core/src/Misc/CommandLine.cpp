#include "Misc/CommandLine.h"
#include <sstream>

namespace Fling
{
	CommandLine& CommandLine::Get()
	{
		// the command line is a singleton... you can only pass
		// in one command line instance for the application's lifetime
		static Fling::CommandLine Instance = {};

		return Instance;
	}

	bool CommandLine::Init(const int32 argc, const char* argv[])
	{
		std::stringstream CmdStream;

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

	std::string_view CommandLine::GetCommandLineData() const
	{
		return CurrentCommandLineData;
	}
} // namespace Fling
