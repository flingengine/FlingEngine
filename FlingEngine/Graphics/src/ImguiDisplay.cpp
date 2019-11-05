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
		
		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);
		ImGui::Begin("Debug");
		VkPhysicalDeviceProperties props = Renderer::Get().GetPhysicalDevice()->GetDeviceProps();
		ImGui::Text("Device: %s", props.deviceName);
		
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

		ImGui::Text("Press 'M' to hide the mouse cursor");
		ImGui::Text("Press 'T' to toggle object rotation");
		ImGui::Text("Press 'WASD' Move");
		ImGui::Text("Right Click and drag to rotate camera");

		ImGui::End();
	}
}