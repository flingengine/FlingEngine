#if WITH_EDITOR

#include "pch.h"
#include "BaseEditor.h"
#include "Renderer.h"

// We have to draw the ImGUI stuff somewhere, so we miind as well keep it all here!
#include "Components/Transform.h"
#include "MeshRenderer.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"
#include <sstream>
#include "ImFileBrowser.hpp"
#include "World.h"

#include <stdio.h> 
#include <string.h> 

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

		void MeshRenderer(Fling::MeshRenderer& t_MeshRend)
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
					// Command buffers must be rebuilt after doing this
					Renderer::Get().SetFrameBufferHasBeenResized(true);

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
					std::string SelectedAsset = FlingPaths::ConvertAbsolutePathToRelative(fileDialog.GetSelected().string());

					t_MeshRend.LoadMaterialFromPath(SelectedAsset);
					// Command buffers must be rebuilt after doing this
					Renderer::Get().SetFrameBufferHasBeenResized(true);

					fileDialog.ClearSelected();
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
				Widgets::MeshRenderer(t);
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
			m_ComponentEditor.renderImGui(t_Reg, m_CompEditorEntityType);
		}
    }

	void BaseEditor::DrawWorldOutline(entt::registry& t_Reg)
	{
		ImGui::Begin("World Outline");

		auto view = t_Reg.view<Transform>();
		for(auto entity: view) 
		{
			std::ostringstream os;
			os << "Entity " << static_cast<UINT64>(entity);
			std::string label = os.str();

			// gets only the components that are going to be used ...
			if(ImGui::Button(label.c_str(), ImVec2( ImGui::GetWindowWidth(), 0.f ) ))
			{
				// Select this eneity for the component editor
				m_CompEditorEntityType = entity;
			}
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
				if (ImGui::MenuItem("Open Level...", "Ctrl+O"))
				{
					// If there are unsaved changes prompt for a save
					OnLoadLevel(FlingConfig::GetString("Game", "StartLevel"));
				}

				if (ImGui::MenuItem("Save Level...", "Ctrl+S"))
				{
					// #TODO Save the current level to it's file path
					OnSaveLevel(FlingConfig::GetString("Game", "StartLevel"));
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
				ImGui::Checkbox("Component Editor", &m_DisplayComponentEditor);
				ImGui::Checkbox("World Outline", &m_DisplayWorldOutline);

				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Help"))
			{
				ImGui::MenuItem("Test A", NULL);
				ImGui::MenuItem("Test B", NULL);
				ImGui::MenuItem("Test C", NULL);
				ImGui::EndMenu();
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

		// Overload this to add custom componented
		m_OwningWorld->OutputLevelFile(t_FileName);
	}

	void BaseEditor::DrawGpuInfo() 
	{
		Timing& Timing = Timing::Get();
		ImGui::Begin("GPU Info");

		ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiCond_FirstUseEver);

		VkPhysicalDeviceProperties props = Renderer::Get().GetPhysicalDevice()->GetDeviceProps();
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