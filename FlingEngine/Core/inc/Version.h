#pragma once

#include "GitVersion.h"

namespace Fling
{
	struct Version
	{
		static int Major;
		static int Minor;
		static int Patch;

		static const std::string& ToString();
	};
}	// namespace Fling