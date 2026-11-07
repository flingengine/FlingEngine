#if WITH_EDITOR

#include "BaseEditor.h"
#include "FirstPersonCamera.h"
#include "Strings/String.h"
#include "VulkanApp.h"

// We have to draw the ImGUI stuff somewhere, so we miind as well keep it all here!
#include "Components/Transform.h"
#include "EditorWidgets.h"
#include "FlingPaths.h"
#include "ImFileBrowser.hpp"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "Logger.h"
#include "MeshRenderer.h"
#include "Timing.h"
#include "TransformUtil.h"
#include "World.h"

#include <stdio.h>
#include <string.h>

namespace Fling
{
    namespace Widgets
    {
        /** A path field with a file browser. Returns true when a new path is picked. */
        bool AssetPicker(const char* t_Label, const char* t_Folder, const char* t_Extension, String& t_Path)
        {
            ImGui::PushID(t_Label);
            ImGui::LabelText(t_Label, "%s", t_Path.empty() ? "None" : t_Path.c_str());

            static ImGui::FileBrowser s_FileDialog;
            static const char* s_OpenFor = nullptr;
            if (ImGui::Button("Select..."))
            {
                s_FileDialog.SetTitle(String("Select ") + t_Label);
                s_FileDialog.SetPwd(std::filesystem::path{FlingPaths::EngineAssetsDir() + "/" + t_Folder});
                s_FileDialog.SetTypeFilters({t_Extension});
                s_FileDialog.Open();
                s_OpenFor = t_Label;
            }

            bool bPicked = false;
            if (s_OpenFor == t_Label)
            {
                s_FileDialog.Display();
                if (s_FileDialog.HasSelected())
                {
                    t_Path = FlingPaths::ConvertAbsolutePathToRelative(s_FileDialog.GetSelected().string());
                    s_FileDialog.ClearSelected();
                    s_OpenFor = nullptr;
                    bPicked = true;
                }
            }
            ImGui::PopID();
            return bPicked;
        }

        void MeshRenderer(Fling::MeshRenderer& t_MeshRend)
        {
            // The renderer sees the new paths in the next snapshot and makes a new proxy
            AssetPicker("Model", "Models", ".obj", t_MeshRend.Mesh);
            AssetPicker("Material", "Materials", ".mat", t_MeshRend.Material);
        }
    } // namespace Widgets

    void BaseEditor::Init(Fling::EntityStorage& t_Reg, World* t_World, Game* t_Game)
    {
        m_OwningWorld = t_World;
        m_WorldSingletons.SetWorld(t_World);
        m_Game = t_Game;
        m_EntityEditor.SetWorld(t_World, &m_Widgets);
        (void)t_Reg;
        RegisterWidgets(m_Widgets);
    }

    void BaseEditor::RegisterWidgets(EditorWidgets& t_Widgets)
    {
        // The other components use the inspector made from their reflected fields.
        // Transform stores a quaternion, so it is edited as Euler angles in degrees.
        t_Widgets.Override<Fling::Transform>(
            [](Fling::EntityStorage&, Fling::Entity, Fling::Transform& t_Trans)
            {
                ImGui::DragFloat3("Position", &t_Trans.Position.x, 0.05f);
                glm::vec3 Degrees = TransformUtil::ToEulerDegrees(t_Trans.Rotation);
                if (ImGui::DragFloat3("Rotation", &Degrees.x, 0.5f))
                {
                    t_Trans.Rotation = TransformUtil::FromEulerDegrees(Degrees);
                }
                ImGui::DragFloat3("Scale", &t_Trans.Scale.x, 0.05f);
            });

        // MeshRenderer saves asset paths by hand, so it gets file pickers.
        t_Widgets.Override<Fling::MeshRenderer>([](Fling::EntityStorage&, Fling::Entity, Fling::MeshRenderer& t_Mesh)
                                                { Widgets::MeshRenderer(t_Mesh); });
    }

    void BaseEditor::Draw(Fling::EntityStorage& t_Reg, float DeltaTime)
    {
        DrawFileMenu();

        if (m_DisplayGPUInfo)
        {
            DrawGpuInfo();
        }

        if (m_DisplayWorldOutline)
        {
            DrawWorldOutline(t_Reg);
        }

        if (m_DisplayComponentEditor)
        {
            DrawEntityEditor(t_Reg);
        }

        if (m_DisplayWindowOptions)
        {
            DrawWindowOptions();
        }

        if (m_DisplayCameraOptions)
        {
            DrawCameraOptions();
        }

        m_InputDevices.Draw(t_Reg);
        m_WorldSingletons.Draw(t_Reg);
    }

    void BaseEditor::DrawCameraOptions()
    {
        ImGui::Begin("Camera Options");

        if (FirstPersonCamera* Cam = VulkanApp::Get().GetCamera())
        {
            ImGui::SetWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
            ImGui::InputFloat("Aspect Ratio", &Cam->m_aspectRatio);
            ImGui::InputFloat("NearPlane", &Cam->m_nearPlane);
            ImGui::InputFloat("FarPlane", &Cam->m_farPlane);
            ImGui::InputFloat("FOV", &Cam->m_fieldOfView);
            ImGui::InputFloat("Gamma", &Cam->m_Gamma);
            ImGui::InputFloat("Exposure", &Cam->m_Exposure);
            ImGui::InputFloat("Speed", &Cam->m_speed);
        }

        ImGui::End();
    }

    void BaseEditor::DrawWorldOutline(Fling::EntityStorage& t_Reg)
    {
        m_WorldOutline.Draw(t_Reg);
    }

    void BaseEditor::DrawEntityEditor(Fling::EntityStorage& t_Reg)
    {
        // Spawn the entity editor for whatever is currently selected in the outline
        m_EntityEditor.SetTargetEntity(m_WorldOutline.GetSelectedEntity());
        m_EntityEditor.Draw(t_Reg);
    }

    void BaseEditor::DrawWindowOptions()
    {
        ImGui::Begin("Window Options");
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        // Dropdown for windowed, borderless, etc
        static const char* WindowOpts[] = {"Fullscreen", "Windowed", "BorderlessWindowed"};
        static const char* CurSelection = nullptr;

        if (ImGui::BeginCombo("Window Mode", CurSelection))
        {
            for (int n = 0; n < IM_ARRAYSIZE(WindowOpts); n++)
            {
                bool is_selected =
                    (CurSelection ==
                     WindowOpts[n]); // You can store your selection however you want, outside or inside your objects
                if (ImGui::Selectable(WindowOpts[n], is_selected))
                {
                    CurSelection = WindowOpts[n];
                }

                if (is_selected)
                {
                    ImGui::SetItemDefaultFocus();
                }
            }
            ImGui::EndCombo();
        }

        ImGui::End();
    }

    void BaseEditor::DrawFileMenu()
    {
        ImGuiWindowFlags corner = ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                                  ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize |
                                  ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoTitleBar |
                                  ImGuiWindowFlags_NoBackground;

        bool isOpen = true;
        ImGui::Begin("File Options", &isOpen, corner);
        ImGui::SetWindowPos(ImVec2(0, 0), true);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                // Loading or saving during play would change or save the play state,
                // and the stop would then reset the level again
                const bool bLevelEditLocked =
                    !m_OwningWorld->IsReadyForPlay() || m_OwningWorld->HasPendingPlayRequest();
                if (bLevelEditLocked)
                {
                    ImGui::TextDisabled("Stop play to open or save levels");
                }
                ImGui::BeginDisabled(bLevelEditLocked);

                static ImGui::FileBrowser fileDialog;
                static bool ShouldLoadLevel = false;
                static bool ShouldSaveLevel = false;

                if (ImGui::Button("Open Level..."))
                {
                    ShouldLoadLevel = true;
                    ShouldSaveLevel = false;

                    fileDialog.SetTitle("Select Level to Load...");
                    static std::filesystem::path p{FlingPaths::EngineAssetsDir() + "/Levels"};
                    fileDialog.SetPwd(p);

                    fileDialog.SetTypeFilters({".json"});
                    fileDialog.Open();
                }

                if (ImGui::Button("Save Level..."))
                {
                    ShouldSaveLevel = true;
                    ShouldLoadLevel = false;

                    fileDialog.SetTitle("Select Save Destination...");
                    static std::filesystem::path p{FlingPaths::EngineAssetsDir() + "/Levels"};
                    fileDialog.SetPwd(p);

                    fileDialog.SetTypeFilters({".json"});
                    fileDialog.Open();
                }

                fileDialog.Display();
                if (fileDialog.HasSelected())
                {
                    String SelectedAsset = FlingPaths::ConvertAbsolutePathToRelative(fileDialog.GetSelected().string());

                    if (ShouldLoadLevel)
                    {
                        OnLoadLevel(SelectedAsset);
                        ShouldLoadLevel = false;
                    }
                    else if (ShouldSaveLevel)
                    {
                        OnSaveLevel(SelectedAsset);
                        ShouldSaveLevel = false;
                    }

                    fileDialog.ClearSelected();
                }

                if (ImGui::MenuItem("New Level", "Ctrl+N"))
                {
                    assert(m_OwningWorld);
                    m_OwningWorld->LoadLevelFile("Levels/EmptyLevel.json");
                }

                ImGui::EndDisabled();
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::Checkbox("GPU Info", &m_DisplayGPUInfo);
                ImGui::Checkbox("Camera Options", &m_DisplayCameraOptions);
                ImGui::Checkbox("Input Devices", &m_InputDevices.ShowWindowFlag());
                ImGui::Checkbox("World Singletons", &m_WorldSingletons.ShowWindowFlag());
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Preferences"))
            {
                if (ImGui::Selectable("Window Options"))
                {
                    m_DisplayWindowOptions = true;
                }
                ImGui::EndMenu();
            }

            DrawPlayControls();

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void BaseEditor::DrawPlayControls()
    {
        assert(m_OwningWorld);

        if (m_OwningWorld->HasPendingPlayRequest())
        {
            // The request runs on the next world update
            ImGui::BeginDisabled();
            ImGui::Button(m_OwningWorld->IsPlaying() ? "Stopping..." : "Starting...");
            ImGui::EndDisabled();
        }
        else if (m_OwningWorld->IsReadyForPlay())
        {
            if (ImGui::Button("Play"))
            {
                m_OwningWorld->RequestGameStart();
            }
        }
        else if (m_OwningWorld->IsPlaying() || m_OwningWorld->IsPaused())
        {
            if (ImGui::Button("Stop"))
            {
                // The world calls OnStopGame, then resets the level to how it was at Play
                m_OwningWorld->RequestGameStop();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Playing (Esc to stop)");
        }
    }

    void BaseEditor::OnLoadLevel(String t_FileName)
    {
        assert(m_OwningWorld);

        // File pop up
        F_LOG_TRACE("Load file {}", t_FileName);

        m_OwningWorld->LoadLevelFile(t_FileName);
    }

    void BaseEditor::OnSaveLevel(String t_FileName)
    {
        assert(m_OwningWorld);

        // File pop up to load the level file
        F_LOG_TRACE("Save to file {}", t_FileName);

        m_OwningWorld->OutputLevelFile(t_FileName);
    }

    void BaseEditor::DrawGpuInfo()
    {
        Timing& Timing = Timing::Get();
        ImGui::Begin("GPU Info");

        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        const String DeviceName = VulkanApp::Get().GetPhysicalDeviceName();
        ImGui::Text("Device: %s", DeviceName.c_str());

        // Update fps graph
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

        if (m_DisplayGPUInfo)
        {
            ImGui::Text("FPS: %f", frameTime);
            ImGui::PlotLines(
                "FPS", &fpsGraph[0], fpsGraph.size(), 0, "", m_FrameTimeMin, m_FrameTimeMax, ImVec2(0, 80));
        }
        ImGui::End();
    }
} // namespace Fling

#endif // WITH_EDITOR
