#pragma once
#include "pch.h"
#include "GitVersion.h"

namespace Fling
{
	struct FLING_API Version
	{
		static int Major;
		static int Minor;
		static int Patch;

		static const std::string& ToString();
	};
}	// namespace Fling