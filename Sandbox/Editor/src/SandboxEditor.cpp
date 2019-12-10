#if WITH_EDITOR

#include "pch.h"
#include "SandboxEditor.h"

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

		ImGui::End();

    }
}   // namespace Sandbox

#endif // WITH_EDITOR