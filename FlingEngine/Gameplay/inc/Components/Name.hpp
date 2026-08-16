#pragma once

#include "JsonArchive.h"

#include <string>

namespace Fling
{
	struct NameComponent
	{
		std::string Name;

		void Serialize(JsonArchive& Ar)
		{
			Ar << MakeNVP("Name", Name);
		}
	};
}	// namespace Fling
