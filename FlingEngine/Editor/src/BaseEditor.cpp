#if WITH_EDITOR

#include "pch.h"
#include "BaseEditor.h"
#include "VulkanApp.h"
#include "PhyscialDevice.h"
#include "FirstPersonCamera.h"

// We have to draw the ImGUI stuff somewhere, so we miind as well keep it all here!
#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "ScriptComponent.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include "ImFileBrowser.hpp"
#include "World.h"
#include "EditableComponent.h"

#include <stdio.h> 
#include <string.h> 
#include <sstream>

namespace Fling
{
    namespace Widgets
    {
        void Transform(Fling::Transform& t)
        {
            ImGui::InputFloat3( "Position", ( float* ) &t.m_Pos );
            ImGui::InputFloat3( "Scale", ( float* )  &t.m_Scale );
            ImGui::InputFloat3( "Rotation", ( float* )  &t.m_Rotation );
        }

        void PointLight(Fling::PointLight& t_Light)
        {
            ImGui::ColorEdit3( "Color", ( float* ) &t_Light.DiffuseColor );
            ImGui::InputFloat( "Range", &t_Light.Range );
            ImGui::InputFloat( "Intensity", &t_Light.Intensity );
        }

        void DirectionalLight(Fling::DirectionalLight& t_Light)
        {
            ImGui::ColorEdit3( "Color", ( float* ) &t_Light.DiffuseColor );
            ImGui::InputFloat3( "Direction", ( float* ) &t_Light.Direction );
            ImGui::InputFloat( "Intensity", &t_Light.Intensity );
        }

        void MeshRenderer(Fling::MeshRenderer& t_MeshRend, entt::registry& t_Reg, entt::entity t_Entity)
        {
            // Model -----------------------
            {
                std::string ModelName = "None";
                if (t_MeshRend.m_Model)
                {
                    ModelName = t_MeshRend.m_Model->GetGuidString();
                }

                ImGui::LabelText("Model", ModelName.c_str(), "%s");

                // Show file browser
                static ImGui::FileBrowser fileDialog;
                if(ImGui::Button("Select Model"))
                {
                    fileDialog.SetTitle("Select Model...");
                    std::filesystem::path p { FlingPaths::EngineAssetsDir() + "/Models" };
                    fileDialog.SetPwd(p);

                    fileDialog.SetTypeFilters({ ".obj" });
                    fileDialog.Open();
                }

                fileDialog.Display();
                if(fileDialog.HasSelected())
                {
                    std::string SelectedAsset = FlingPaths::ConvertAbsolutePathToRelative(fileDialog.GetSelected().string());

                    t_MeshRend.LoadModelFromPath(SelectedAsset);

                    fileDialog.ClearSelected();
                }
            }

            // Material ----------------------
            {
                std::string MaterialName = "None";
                if (t_MeshRend.m_Material)
                {
                    MaterialName = t_MeshRend.m_Material->GetGuidString();
                }
                
                const char* m = MaterialName.c_str();
                ImGui::LabelText("Material", m, "%s");

                // Show file browser
                static ImGui::FileBrowser fileDialog;
                if(ImGui::Button("Select Material"))
                {
                    fileDialog.SetTitle("Select Material...");
                    std::filesystem::path p { FlingPaths::EngineAssetsDir() + "/Materials" };
                    fileDialog.SetPwd(p);


                    fileDialog.SetTypeFilters({ ".mat" });
                    fileDialog.Open();
                }

                fileDialog.Display();
                if(fileDialog.HasSelected())
                {
                    std::string ModelName = t_MeshRend.m_Model->GetGuidString();
                    std::string SelectedAsset = FlingPaths::ConvertAbsolutePathToRelative(fileDialog.GetSelected().string());

                    t_MeshRend.LoadMaterialFromPath(SelectedAsset);
                    t_Reg.replace<Fling::MeshRenderer>(t_Entity, ModelName, SelectedAsset);

                    fileDialog.ClearSelected();
                }

				// Material type --------

				const char* items[] = { "AAAA", "BBBB", "CCCC", "DDDD", "EEEE", "FFFF", "GGGG", "HHHH", "IIII", "JJJJ", "KKKK", "LLLLLLL", "MMMM", "OOOOOOO", "PPPP", "QQQQQQQQQQ", "RRR", "SSSS" };
				static const char* current_item = NULL;

				if (ImGui::BeginCombo("##combo", current_item)) // The second parameter is the label previewed before opening the combo.
				{
					for (int n = 0; n < IM_ARRAYSIZE(items); n++)
					{
						bool is_selected = (current_item == items[n]); // You can store your selection however you want, outside or inside your objects
						if (ImGui::Selectable(items[n], is_selected))
						{
							current_item = items[n];
							if (is_selected)
							{
								ImGui::SetItemDefaultFocus();   // You may set the initial focus when opening the combo (scrolling + for keyboard navigation support)

							}
						}
					}
					ImGui::EndCombo();
				}
            }
        }
    }

    void BaseEditor::RegisterComponents(entt::registry& t_Reg)
    {
        m_ComponentEditor.registerTrivial<Fling::Transform>(t_Reg, "Transform");
        m_ComponentEditor.registerComponentWidgetFn(
            t_Reg.type<Fling::Transform>(),
            [](entt::registry& reg, auto e) 
            {
                auto& t = reg.get<Fling::Transform>(e);
                Widgets::Transform(t);
            }
        );

        m_ComponentEditor.registerTrivial<Fling::PointLight>(t_Reg, "PointLight");
        m_ComponentEditor.registerComponentWidgetFn(
            t_Reg.type<Fling::PointLight>(),
            [](entt::registry& reg, auto e) 
            {
                auto& t = reg.get<Fling::PointLight>(e);
                Widgets::PointLight(t);
            }
        );

        m_ComponentEditor.registerTrivial<Fling::DirectionalLight>(t_Reg, "Directional Light");
        m_ComponentEditor.registerComponentWidgetFn(
            t_Reg.type<Fling::DirectionalLight>(),
            [](entt::registry& reg, auto e) 
            {
                auto& t = reg.get<Fling::DirectionalLight>(e);
                Widgets::DirectionalLight(t);
            }
        );

        m_ComponentEditor.registerTrivial<Fling::MeshRenderer>(t_Reg, "Mesh Renderer");
        m_ComponentEditor.registerComponentWidgetFn(
            t_Reg.type<Fling::MeshRenderer>(),
            [](entt::registry& reg, auto e)
            {
                auto& t = reg.get<Fling::MeshRenderer>(e);
                Widgets::MeshRenderer(t, reg, e);
            }
        );
    }

    void BaseEditor::Draw(entt::registry& t_Reg, float DeltaTime)
    {        
        DrawFileMenu();

        if (m_DisplayGPUInfo)
        {
            DrawGpuInfo();
        }

        if(m_DisplayWorldOutline)
        {
            DrawWorldOutline(t_Reg);
        }

        if(m_DisplayComponentEditor)
        {
            DrawComponentEditor(t_Reg);
        }

        if (m_DisplayWindowOptions)
        {
            DrawWindowOptions();
        }

        if(m_DisplayCameraOptions)
        {
            DrawCameraOptions();
        }
    }

    void BaseEditor::DrawCameraOptions()
    {
		ImGui::Begin("Camera Options");

        if(FirstPersonCamera* Cam = VulkanApp::Get().GetCamera())
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

    void BaseEditor::DrawWorldOutline(entt::registry& t_Reg)
    {
        ImGui::Begin("World Outline");

		ImGui::SetWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
		ImGui::SetWindowPos(ImVec2(0.0f, 30.0f), ImGuiCond_FirstUseEver);

        auto view = t_Reg.view<EditableComponent>();
        for(entt::entity entity : view) 
        {
            const bool bStartedSelected = (m_CompEditorEntityType == entity);
            
            std::ostringstream os;
            os << "Entity " << static_cast<uint64>(entity);
            std::string label = os.str();

            if (ImGui::Button(" - "))
            {
                if (ImGui::IsItemHovered())
                {
                    ImGui::Text("Hovered");
                }

                F_LOG_TRACE("Delete {}", label);
                t_Reg.destroy(entity);
            }

            ImGui::SameLine();
            
            // If the entity is currently selected, then give it a different color in the editor
            if(bStartedSelected)
            {
                ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(7.0f, 0.6f, 0.6f));
            }

            // gets only the components that are going to be used ...
            if(ImGui::Button(label.c_str(), ImVec2( ImGui::GetWindowWidth(), 0.f ) ))
            {
                // Select this eneity for the component editor
                m_CompEditorEntityType = entity;
            }

            if(bStartedSelected)
            {
                ImGui::PopStyleColor(1);
            }
        }

        ImGui::End();
    }

    void BaseEditor::DrawComponentEditor(entt::registry& t_Reg)
    {
		// Set the window options for the component editor
		ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
		ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowWidth(), 30.0f), ImGuiCond_FirstUseEver);

		m_ComponentEditor.renderImGui(t_Reg, m_CompEditorEntityType);

        // Make sure that each entity has a transform so that they show up in the editor window
        if(m_CompEditorEntityType != entt::null)
        {
            if(!t_Reg.has<EditableComponent>(m_CompEditorEntityType))
            {
				t_Reg.assign<EditableComponent>(m_CompEditorEntityType);
            }
        }
    }

    void BaseEditor::DrawWindowOptions()
    {
        ImGui::Begin("Window Options");
        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

        // Dropdown for windowed, borderless, etc
		static const char* WindowOpts[] = { "Fullscreen", "Windowed", "BorderlessWindowed" };
        static const char* CurSelection = nullptr;

		if (ImGui::BeginCombo("Window Mode", CurSelection))
		{
			for (int n = 0; n < IM_ARRAYSIZE(WindowOpts); n++)
            {
				bool is_selected = (CurSelection == WindowOpts[n]); // You can store your selection however you want, outside or inside your objects
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
        ImGuiWindowFlags corner =
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoResize |
            ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_MenuBar |
            ImGuiWindowFlags_NoTitleBar | 
            ImGuiWindowFlags_NoBackground;

        bool isOpen = true;
        ImGui::Begin("File Options", &isOpen, corner);
        ImGui::SetWindowPos(ImVec2(0, 0), true);

        if (ImGui::BeginMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                static ImGui::FileBrowser fileDialog;
                static bool ShouldLoadLevel = false;
                static bool ShouldSaveLevel = false;

                if (ImGui::Button("Open Level..."))
                {
                    ShouldLoadLevel = true;
                    ShouldSaveLevel = false;

                    fileDialog.SetTitle("Select Level to Load...");
                    static std::filesystem::path p{ FlingPaths::EngineAssetsDir() + "/Levels" };
                    fileDialog.SetPwd(p);

                    fileDialog.SetTypeFilters({ ".json" });
                    fileDialog.Open();
                }

                if (ImGui::Button("Save Level..."))
                {
                    ShouldSaveLevel = true;
                    ShouldLoadLevel = false;

                    fileDialog.SetTitle("Select Save Destination...");
                    static std::filesystem::path p{ FlingPaths::EngineAssetsDir() + "/Levels" };
                    fileDialog.SetPwd(p);

                    fileDialog.SetTypeFilters({ ".json" });
                    fileDialog.Open();
                }

                fileDialog.Display();
                if (fileDialog.HasSelected())
                {
                    std::string SelectedAsset = FlingPaths::ConvertAbsolutePathToRelative(fileDialog.GetSelected().string());

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

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Windows"))
            {
                ImGui::Checkbox("GPU Info", &m_DisplayGPUInfo);
				ImGui::Checkbox("Camera Options", &m_DisplayCameraOptions);
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

            if (m_OwningWorld->IsReadyForPlay())
            {
				if (ImGui::Button("Play Game"))
				{
					// Tell the world that we should start the game logic
					m_OwningWorld->RequestGameStart();
				}
            }
            else if(m_OwningWorld->IsPlaying())
            {
				if (ImGui::Button("Stop Game"))
				{
					// Tell the world to stop game play logic
					m_OwningWorld->RequestGameStop();
				}
            }

            ImGui::EndMenuBar();
        }

        ImGui::End();
    }

    void BaseEditor::OnLoadLevel(std::string t_FileName)
    {
        assert(m_OwningWorld);

        // File pop up
        F_LOG_TRACE("Load file {}", t_FileName);

        m_OwningWorld->LoadLevelFile(t_FileName);
    }

    void BaseEditor::OnSaveLevel(std::string t_FileName)
    {
        assert(m_OwningWorld);

        // File pop up to load the level file 
        F_LOG_TRACE("Save to file {}", t_FileName);

        // Add template arguments here to add custom components to the level files
        m_OwningWorld->OutputLevelFile(t_FileName);
    }

    void BaseEditor::DrawGpuInfo() 
    {
        Timing& Timing = Timing::Get();
        ImGui::Begin("GPU Info");

        ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

		PhysicalDevice* PhysDev = VulkanApp::Get().GetPhysicalDevice();
		assert(PhysDev);
        VkPhysicalDeviceProperties props = PhysDev->GetDeviceProps();
        ImGui::Text("Device: %s", props.deviceName);

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
            ImGui::PlotLines("FPS", &fpsGraph[0], fpsGraph.size(), 0, "", m_FrameTimeMin, m_FrameTimeMax, ImVec2(0, 80));
        }
        ImGui::End();
    }
}   // namespace Fling

#endif  // WITH_EDITOR