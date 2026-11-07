#pragma once

#include "Ecs/Component.h"

#include <string>

namespace Fling
{
	struct NameComponent
	{
		static constexpr ComponentInfo Component{.Name = "NameComponent"};

		std::string Name;
	};
}	// namespace Fling
