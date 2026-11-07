#if WITH_EDITOR

#include "FirstPersonCamera.h"
#include "Logger.h"
#include "SandboxEditor.h"
#include "Strings/String.h"
#include "VulkanApp.h"
#include "World.h"

namespace Sandbox
{
    void SandboxEditor::Draw(Fling::EntityStorage& t_Reg, float DeltaTime)
    {
		Fling::BaseEditor::Draw(t_Reg, DeltaTime);

		ImGui::Begin("Demo Info");
		// Set it up in a nice spot
		ImGui::SetWindowPos(ImVec2(0.0f, 500.0f), ImGuiCond_FirstUseEver);

        ImGui::Text("Press 'Play' to start, 'Esc' or 'Stop' to stop");
        ImGui::Text("Press 'M' to hide the mouse cursor");
		ImGui::Text("Press 'Space' to move lights around");
        ImGui::Text("Press 'T' to rotate the models");
        ImGui::Text("Press 'WASD' Move");
        ImGui::Text("Right Click and drag to rotate camera");

        // Gamma correction settings
		float GammaLevel = Fling::VulkanApp::Get().GetCamera()->GetGamma();
		ImGui::SliderFloat("Gamma", &GammaLevel, .01f, 5.0f);
		Fling::VulkanApp::Get().GetCamera()->SetGamma(GammaLevel);

		// Exposure settings
		float ExposureSetting = Fling::VulkanApp::Get().GetCamera()->GetExposure();
		ImGui::SliderFloat("Exposure", &ExposureSetting, 0.0f, 100.0f);
		Fling::VulkanApp::Get().GetCamera()->SetExposure(ExposureSetting);

		ImGui::End();
    }

	void SandboxEditor::OnLoadLevel(Fling::String t_FileName)
	{
		F_LOG_TRACE("Load file {}", t_FileName);

		m_OwningWorld->LoadLevelFile(t_FileName);
	}

	void SandboxEditor::OnSaveLevel(Fling::String t_FileName)
	{
		// File pop up to load the level file 
		F_LOG_TRACE("Save to file {}", t_FileName);

		m_OwningWorld->OutputLevelFile(t_FileName);
	}
}   // namespace Sandbox

#endif // WITH_EDITOR