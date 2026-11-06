#pragma once

#include "Ecs/Component.h"
#include "JsonArchive.h"

#include <string>

namespace Fling
{
	struct NameComponent
	{
		static constexpr ComponentInfo Component{.Name = "NameComponent"};

		std::string Name;

		void Serialize(JsonArchive& Ar)
		{
			Ar << MakeNVP("Name", Name);
		}
	};
}	// namespace Fling
