#include "ImguiDisplay.h"
#include "Renderer.h"
#include "Timing.h"

namespace Fling
{
	ImguiDisplay::ImguiDisplay()
	{
		fpsGraph = { 0 };
	}


	void ImguiDisplay::NewFrame()
	{
		Timing& Timing = Timing::Get();
		ImGuiIO& io = ImGui::GetIO();
		ImVec4 clear_color = ImColor(114, 144, 154);
		
		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Debug");
		ImGui::Text("Device: %s", Renderer::Get().GetPhysicalDevice()->GetDeviceProps().deviceName);
		
		//Update fps graph
		std::rotate(fpsGraph.begin(), fpsGraph.begin() + 1, fpsGraph.end());
		float frameTime = static_cast<float>(Timing.GetFrameCount());
		fpsGraph.back() = frameTime;

		if (frameTime < frameTimeMin) 
		{
			frameTimeMin = frameTime;
		}
		if (frameTime > frameTimeMax) 
		{
			frameTimeMax = frameTime;
		}
		
		ImGui::Text("FPS: %f", frameTime);
		ImGui::PlotLines("FPS", &fpsGraph[0], fpsGraph.size(), 0, "", frameTimeMin, frameTimeMax, ImVec2(0, 80));

		ImGui::Checkbox("Mouse click left", &io.MouseDown[0]);
		ImGui::Checkbox("Mouse click right", &io.MouseDown[1]);
		ImGui::End();
	}
}