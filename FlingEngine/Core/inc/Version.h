#pragma once
#include "CoreAPI.h"
#include "GitVersion.h"
#include "Platform.h"
#include "Strings/String.h"

namespace Fling
{
	struct CORE_API Version
	{
		int Major;
		int Minor;
		int Patch;

		String ToString() const;
		
		static Version EngineVersion;
	};
}	// namespace Fling