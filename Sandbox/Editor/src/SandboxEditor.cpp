#if WITH_EDITOR

#include "pch.h"
#include "SandboxEditor.h"
#include "World.h"
#include "Mover.h"

namespace Sandbox
{
    void SandboxEditor::Draw(entt::registry& t_Reg, float DeltaTime)
    {
        // #TODO: Draw some bois
		Fling::BaseEditor::Draw(t_Reg, DeltaTime);

		ImGui::Begin("Demo Info");
		ImGui::Text("Press 'M' to hide the mouse cursor");
		ImGui::Text("Press 'Space' to move lights around");
		ImGui::Text("Press 'WASD' Move");
		ImGui::Text("Right Click and drag to rotate camera");
        ImGui::Text("Press 'WASD' Move");

        ImGui::Text("Press '2' for fullscreen");
        ImGui::Text("Press '3' for windowed");
        ImGui::Text("Press '4' for borderless");

		ImGui::End();
    }

	void SandboxEditor::OnLoadLevel(std::string t_FileName)
	{
		F_LOG_TRACE("Load file {}", t_FileName);

		m_OwningWorld->LoadLevelFile<Mover, Rotator>(t_FileName);
	}

	void SandboxEditor::OnSaveLevel(std::string t_FileName)
	{
		// File pop up to load the level file 
		F_LOG_TRACE("Save to file {}", t_FileName);

		m_OwningWorld->OutputLevelFile<Mover, Rotator>(t_FileName);
	}
}   // namespace Sandbox

#endif // WITH_EDITOR