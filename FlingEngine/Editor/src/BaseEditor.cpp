#if WITH_EDITOR

#include "pch.h"
#include "BaseEditor.h"
#include "Renderer.h"

namespace Fling
{
    void BaseEditor::Draw(const entt::registry& t_Reg, float DeltaTime)
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

		if (frameTime < m_FrameTimeMin) 
		{
			m_FrameTimeMin = frameTime;
		}
		if (frameTime > m_FrameTimeMax) 
		{
			m_FrameTimeMax = frameTime;
		}
		
		ImGui::Text("FPS: %f", frameTime);
		ImGui::PlotLines("FPS", &fpsGraph[0], fpsGraph.size(), 0, "", m_FrameTimeMin, m_FrameTimeMax, ImVec2(0, 80));

		ImGui::Checkbox("Mouse click left", &io.MouseDown[0]);
		ImGui::Checkbox("Mouse click right", &io.MouseDown[1]);

		ImGui::Text("Press 'M' to hide the mouse cursor");
		ImGui::Text("Press 'T' to toggle object rotation");
		ImGui::Text("Press 'WASD' Move");
		ImGui::Text("Right Click and drag to rotate camera");

		ImGui::End();
    }
}   // namespace Fling

#endif  // WITH_EDITOR