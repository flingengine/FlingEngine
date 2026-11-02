#pragma once
#include "CoreAPI.h"
#include "GitVersion.h"
#include "Platform.h"

#include <string>

namespace Fling
{
	struct CORE_API Version
	{
		int Major;
		int Minor;
		int Patch;

		std::string ToString() const;
		
		static Version EngineVersion;
	};
}	// namespace Fling