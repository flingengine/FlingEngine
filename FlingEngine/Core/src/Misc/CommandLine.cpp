#include "Misc/CommandLine.h"
#include <sstream>

namespace Fling
{
	std::string CommandLine::CurrentCommandLine;
	
	void CommandLine::Set(const std::string& CmdLine)
	{
		CurrentCommandLine = CmdLine;
	}

	std::string CommandLine::BuildFromArgs(int32 argc, const char* argv[])
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
		return CmdStream.str();
	}

	bool CommandLine::Parse(const std::string& InKey)
	{
		
		return false;
	}

	bool CommandLine::HasParam(const std::string& Param)
	{
		std::size_t found = CurrentCommandLine.find(Param);

		return found != std::string::npos;
	}

} // namespace Fling
