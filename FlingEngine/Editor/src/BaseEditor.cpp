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
#include "PresentMode.h"
#include "RunControls.h"
#include "Screenshot.h"
#include "SimRate.h"
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

    std::shared_ptr<World> BaseEditor::OwningWorld() const
    {
        std::shared_ptr<World> Result = m_OwningWorld.lock();
        assert(Result && "The editor is used before Init, or after the world is gone");
        return Result;
    }

    void BaseEditor::Init(Fling::EntityStorage& t_Reg,
                          const std::shared_ptr<World>& t_World,
                          const std::shared_ptr<Game>& t_Game,
                          RunControls& t_Controls)
    {
        m_Controls = &t_Controls;
        // A plain click on a drag field opens a text box; dragging still scrubs the value.
        ImGui::GetIO().ConfigDragClickToInputText = true;

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

        if (m_DisplayGpuOptions)
        {
            DrawGpuOptions();
        }

        // In play the panels are read only in every exec mode: an edit would change the
        // simulation outside a tick. In Inline and Threaded play they show the published copy
        const std::shared_ptr<World> ShownWorld = OwningWorld();
        const bool bReadOnly = ShownWorld->IsPlaying() || ShownWorld->IsPresentingCopy();
        Fling::EntityStorage& Shown = ShownWorld->IsPresentingCopy() ? ShownWorld->GetPresentedStorage() : t_Reg;
        m_WorldOutline.SetReadOnly(bReadOnly);

        if (m_DisplayWorldOutline)
        {
            DrawWorldOutline(Shown);
        }

        ImGui::BeginDisabled(bReadOnly);
        if (m_DisplayComponentEditor)
        {
            DrawEntityEditor(Shown);
        }
        ImGui::EndDisabled();

        if (m_DisplayWindowOptions)
        {
            DrawWindowOptions();
        }

        if (m_DisplayCameraOptions)
        {
            DrawCameraOptions();
        }

        // The input panel runs on the thread that polls input (the input writer), and the
        // game thread only claims replay cuts and notes recorded cuts, so it works in play
        m_InputDevices.Draw(Shown);

        ImGui::BeginDisabled(bReadOnly);
        m_WorldSingletons.Draw(Shown);
        ImGui::EndDisabled();
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
                    !OwningWorld()->IsReadyForPlay() || OwningWorld()->HasPendingPlayRequest();
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
                    OwningWorld()->LoadLevelFile("Levels/EmptyLevel.json");
                }

                ImGui::EndDisabled();

                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4"))
                {
                    OwningWorld()->RequestQuit();
                }
                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::Checkbox("GPU Options", &m_DisplayGpuOptions);
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
        if (OwningWorld()->HasPendingPlayRequest())
        {
            // The request runs on the next world update
            ImGui::BeginDisabled();
            ImGui::Button(OwningWorld()->IsPlaying() ? "Stopping..." : "Starting...");
            ImGui::EndDisabled();
        }
        else if (OwningWorld()->IsReadyForPlay())
        {
            if (ImGui::Button("Play"))
            {
                OwningWorld()->RequestGameStart();
            }
        }
        else if (OwningWorld()->IsPlaying())
        {
            if (ImGui::Button("Stop"))
            {
                // The world calls OnStopGame, then resets the level to how it was at Play
                OwningWorld()->RequestGameStop();
            }
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Playing (Esc to stop)");
        }
    }

    void BaseEditor::OnLoadLevel(String t_FileName)
    {
        // File pop up
        F_LOG_TRACE("Load file {}", t_FileName);

        OwningWorld()->LoadLevelFile(t_FileName);
    }

    void BaseEditor::OnSaveLevel(String t_FileName)
    {
        // File pop up to load the level file
        F_LOG_TRACE("Save to file {}", t_FileName);

        OwningWorld()->OutputLevelFile(t_FileName);
    }

    namespace
    {
        /**
         * A whole number box that shows t_Current until it is edited, and returns true once
         * when an edit is committed (Enter or focus loss), with the typed value in t_Field.
         */
        template <class T_Field>
        bool CommittedNumberBox(const char* t_Label, const uint32 t_Current, T_Field& t_Field)
        {
            if (!t_Field.bEditing)
            {
                t_Field.Value = t_Current;
            }
            ImGui::InputScalar(t_Label, ImGuiDataType_U32, &t_Field.Value);
            t_Field.bEditing = ImGui::IsItemActive();
            return ImGui::IsItemDeactivatedAfterEdit();
        }
    } // namespace

    void BaseEditor::DrawGpuOptions()
    {
        assert(m_Controls && "Init gives the editor the run controls");
        RunControls& Controls = *m_Controls;

        // Right of the World Outline and Entity Editor windows, so none covers another on
        // first use, and wide enough for the limiter and measured lines
        ImGui::SetNextWindowPos(ImVec2(520, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(600, 400), ImGuiCond_FirstUseEver);
        ImGui::Begin("GPU Options");

        const String DeviceName = VulkanApp::Get().GetPhysicalDeviceName();
        ImGui::Text("Device: %s", DeviceName.c_str());

        // The setting; the swap chain is made again with it after the next present
        const char* const VSyncItems[] = {"On", "Off"};
        int VSyncItem = Controls.VSync() ? 0 : 1;
        if (ImGui::Combo("VSync", &VSyncItem, VSyncItems, static_cast<int>(std::size(VSyncItems))))
        {
            m_ControlsError.clear();
            Controls.SetVSync(VSyncItem == 0);
        }
        const PresentMode InUse = VulkanApp::Get().GetPresentMode();
        ImGui::TextDisabled("Present mode in use: %s", PresentModeName(InUse));

        if (CommittedNumberBox("Frame cap (fps)", Controls.MaxFps(), m_FrameCapField))
        {
            Controls.SetMaxFps(m_FrameCapField.Value, m_ControlsError);
        }
        ImGui::TextDisabled("0: no cap, or 1 to %u. %s",
                            RunSettings::MaxMaxFps,
                            Controls.DescribeLimiter(DisplayPacesFrames(InUse)).c_str());

        if (CommittedNumberBox("Simulation rate (Hz)", Controls.SimHz(), m_SimHzField))
        {
            Controls.RequestSimHz(m_SimHzField.Value, m_ControlsError);
        }
        ImGui::TextDisabled("%u to %u ticks a second, from the next tick on", SimRate::MinHz, SimRate::MaxHz);

        bool bInterpolation = Controls.Interpolation();
        if (ImGui::Checkbox("Render interpolation", &bInterpolation))
        {
            m_ControlsError.clear();
            Controls.SetInterpolation(bInterpolation);
        }

        bool bFrustumCulling = Controls.FrustumCulling();
        if (ImGui::Checkbox("Frustum culling", &bFrustumCulling))
        {
            m_ControlsError.clear();
            Controls.SetFrustumCulling(bFrustumCulling);
        }
        bool bOcclusionCulling = Controls.OcclusionCulling();
        if (ImGui::Checkbox("Occlusion culling", &bOcclusionCulling))
        {
            m_ControlsError.clear();
            Controls.SetOcclusionCulling(bOcclusionCulling);
        }
        bool bGpuBlend = Controls.GpuBlend();
        if (ImGui::Checkbox("GPU blend (needs occlusion culling)", &bGpuBlend))
        {
            m_ControlsError.clear();
            Controls.SetGpuBlend(bGpuBlend);
        }
        const CullStats Culled = VulkanApp::Get().GetCullStats();
        ImGui::TextDisabled("Last frame: %u meshes drawn, %u outside the view, %u occluded (hidden by other meshes)",
                            Culled.Drawn,
                            Culled.Culled,
                            Culled.Occluded);

        if (!m_ControlsError.empty())
        {
            ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_ControlsError.c_str());
        }

        ImGui::Separator();
        // The next frame as it is shown, this window included, to Saved/Screenshots
        if (ImGui::Button("Take screenshot"))
        {
            const std::weak_ptr<String> Status = m_ScreenshotStatus;
            const String Path = Screenshot::Request({},
                                                    [Status](const ScreenshotResult& t_Result)
                                                    {
                                                        if (const std::shared_ptr<String> Text = Status.lock())
                                                        {
                                                            *Text = t_Result.Ok ? "Saved " + t_Result.Path
                                                                                : "Not saved: " + t_Result.Error;
                                                        }
                                                    });
            *m_ScreenshotStatus = "Saving " + Path;
        }
        ImGui::SameLine();
        ImGui::TextDisabled("The next frame, with the editor UI, as a PNG");
        if (!m_ScreenshotStatus->empty())
        {
            ImGui::TextWrapped("%s", m_ScreenshotStatus->c_str());
        }

        ImGui::Separator();
        const float Fps = static_cast<float>(Timing::Get().GetFrameCount());
        ImGui::TextWrapped("%s", Controls.DescribeMeasured(Fps).c_str());

        std::rotate(fpsGraph.begin(), fpsGraph.begin() + 1, fpsGraph.end());
        fpsGraph.back() = Fps;
        m_FrameTimeMin = std::min(m_FrameTimeMin, Fps);
        m_FrameTimeMax = std::max(m_FrameTimeMax, Fps);
        ImGui::PlotLines("FPS",
                         fpsGraph.data(),
                         static_cast<int>(fpsGraph.size()),
                         0,
                         "",
                         m_FrameTimeMin,
                         m_FrameTimeMax,
                         ImVec2(0, 80));
        ImGui::End();
    }
} // namespace Fling

#endif // WITH_EDITOR
