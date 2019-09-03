#pragma once
#include "pch.h"
#include "GitVersion.h"

namespace Fling
{
	struct FLING_API Version
	{
		int Major;
		int Minor;
		int Patch;

		std::string ToString() const;
		
		static Version EngineVersion;
	};
}	// namespace Fling