#pragma once

#include "FlingMath.h"
#include "JsonArchive.h"

namespace Fling
{
	/**
	 * Simple representation of a directional light for Fling. Needs to be 16 bytes aligned
	 *          for Vulkan
	 */
	struct alignas(16) DirectionalLight
	{
		alignas(16) glm::vec4 DiffuseColor { 1.0f };
		alignas(16) glm::vec4 Direction { 1.0f, -1.0f, -0.5f, 1.0f  };
		alignas(4)  float Intensity = 1.0f;

		void Serialize(JsonArchive& Ar)
		{
			Ar << MakeNVP("color", DiffuseColor);
			Ar << MakeNVP("direction", Direction);
			Ar << MakeNVP("intensity", Intensity);
		}
	};
}	// namespace Fling
