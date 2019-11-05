#pragma once

#if WITH_IMGUI
#include <imgui.h>
#endif
#include <array>

namespace Fling
{
	class ImguiDisplay
	{
	public:
		ImguiDisplay();
		~ImguiDisplay() {};

		/**
		 * @brief User defined callback to make or change imgui UI
		 * 
		 */
		void NewFrame();

	private:
		std::array<float, 400> fpsGraph{};
		float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
	};
}