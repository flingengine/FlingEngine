#if WITH_EDITOR

#include "CameraStatus.h"
#include "FirstPersonCamera.h"
#include "Logger.h"
#include "Misc/CommandLine.h"
#include "SandboxEditor.h"
#include "Strings/String.h"
#include "VulkanApp.h"
#include "World.h"

namespace Sandbox
{
    void SandboxEditor::Init(Fling::EntityStorage& t_Reg,
                             const std::shared_ptr<Fling::World>& t_World,
                             const std::shared_ptr<Fling::Game>& t_Game,
                             Fling::RunControls& t_Controls)
    {
        Fling::BaseEditor::Init(t_Reg, t_World, t_Game, t_Controls);
        m_DisplayGpuOptions = Fling::CommandLine::Get().GetValueAs<bool>("play", false);
    }

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
        ImGui::Text("Press 'F' to freeze the camera, 'G' for 2 s of stasis, '-' / '=' look speed");
        DrawCameraStatus();

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

    void SandboxEditor::DrawCameraStatus()
    {
        // The frame side's copy (the mirror in Inline and Threaded play), which Sandbox.CameraStatus
        // wrote from published state this frame. It runs only in play.
        const std::shared_ptr<Fling::World> ShownWorld = OwningWorld();
        const CameraStatus* Status =
            ShownWorld->IsPlaying() ? ShownWorld->GetPresentedStorage().TryGetSingleton<CameraStatus>() : nullptr;
        const CameraMode Mode = Status != nullptr ? Status->Mode : CameraMode::None;
        switch (Mode)
        {
        case CameraMode::Free:
            ImGui::Text("Camera: free");
            break;
        case CameraMode::Frozen:
            ImGui::Text("Camera: frozen (F)");
            break;
        case CameraMode::Stasis:
            ImGui::Text("Camera: stasis %.1f s", Status->StasisSecondsLeft);
            break;
        case CameraMode::None:
            break;
        }
    }

    void SandboxEditor::OnLoadLevel(Fling::String t_FileName)
    {
        F_LOG_TRACE("Load file {}", t_FileName);

        OwningWorld()->LoadLevelFile(t_FileName);
    }

    void SandboxEditor::OnSaveLevel(Fling::String t_FileName)
    {
        // File pop up to load the level file
        F_LOG_TRACE("Save to file {}", t_FileName);

        OwningWorld()->OutputLevelFile(t_FileName);
    }
} // namespace Sandbox

#endif // WITH_EDITOR