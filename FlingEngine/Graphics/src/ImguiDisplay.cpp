#include "ImguiDisplay.h"

namespace Fling
{
	ImguiDisplay::ImguiDisplay()
	{
		frameTimes = { 0 };
	}


	void ImguiDisplay::NewFrame()
	{
		Timing& Timing = Timing::Get();
		ImGui::NewFrame();
		ImGuiIO& io = ImGui::GetIO();
		ImVec4 clear_color = ImColor(114, 144, 154);
		ImGui::TextUnformatted("Device name");

		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Debug");
		std::rotate(frameTimes.begin(), frameTimes.begin() + 1, frameTimes.end());
		float frameTime = Timing.GetFrameCount();
		frameTimes.back() = frameTime;
		if (frameTime < frameTimeMin) {
			frameTimeMin = frameTime;
		}
		if (frameTime > frameTimeMax) {
			frameTimeMax = frameTime;
		}
		
		ImGui::PlotLines("Frame Times", &frameTimes[0], 50, 0, "", frameTimeMin, frameTimeMax, ImVec2(0, 80));

		ImGui::Checkbox("Mouse click left", &io.MouseDown[0]);
		ImGui::Checkbox("Mouse click right", &io.MouseDown[1]);
		ImGui::End();

		//ImGui::SetNextWindowPos(ImVec2(650, -1000), ImGuiCond_FirstUseEver);
		ImGui::ShowDemoWindow();
		ImGui::Render();
	}
}