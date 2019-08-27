#include "pch.h"
#include "Version.h"
namespace Fling
{
	Version Version::EngineVersion = { 0, 0 , 1 };

	std::string Version::ToString() const
	{
		std::string VersionString = std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
		return VersionString;
	}

}	// namespace Fling