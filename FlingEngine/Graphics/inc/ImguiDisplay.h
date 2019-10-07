#pragma once

#include <imgui.h>
#include <array>
#include "Timing.h"

namespace Fling
{
	class ImguiDisplay
	{
	public:
		ImguiDisplay();
		~ImguiDisplay() {};

		void NewFrame();

	private:
		std::array<float, 50> frameTimes{};
		float frameTimeMin = 9999.0f, frameTimeMax = 0.0f;
	};
}