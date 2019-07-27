#include "pch.h"
#include "Version.h"
namespace Fling
{
	int Version::Major = 0;
	int Version::Minor = 1;
	int Version::Patch = 1;

	const std::string& Version::ToString()
	{
		static std::string VersionString = std::to_string(Major) + "." + std::to_string(Minor) + "." + std::to_string(Patch);
		return VersionString;
	}

}	// namespace Fling