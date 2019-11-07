#if WITH_EDITOR

#include "pch.h"
#include "BaseEditor.h"
#include "Renderer.h"

// We have to draw the ImGUI stuff somewhere, so we miind as well keep it all here!
#include "Components/Transform.h"
#include "Lighting/DirectionalLight.hpp"
#include "Lighting/PointLight.hpp"

namespace Fling
{
	namespace Widgets
	{
		void Transform(Fling::Transform& t)
		{
			ImGui::DragFloat3( "Position", ( float* ) &t.m_Pos );
			ImGui::DragFloat3( "Scale", ( float* )  &t.m_Pos );
			ImGui::DragFloat3( "Rotation", ( float* )  &t.m_Rotation );
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
			ImGui::DragFloat3( "Direction", ( float* ) &t_Light.Direction );
			ImGui::InputFloat( "Intensity", &t_Light.Intensity );
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
	}

	void BaseEditor::Draw(entt::registry& t_Reg, float DeltaTime)
    {		
		DrawFileMenu();

		if (m_DisplayGPUInfo)
		{
			DrawGpuInfo();
		}

		m_ComponentEditor.renderImGui(t_Reg, m_CompEditorEntityType);
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

					// #TODO Open a level 
				}

				if (ImGui::MenuItem("Save Level...", "Ctrl+S"))
				{
					// #TODO Save the current level to it's file path
				}

				if (ImGui::MenuItem("New Level", "Ctrl+N"))
				{
					// If there are unsaved changes prompt for a save

					// #TODO Open a new level 
				}
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Windows"))
			{
				ImGui::Checkbox("GPU Info", &m_DisplayGPUInfo);

				ImGui::MenuItem("Test B", NULL);
				ImGui::MenuItem("Test C", NULL);
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
	}
}   // namespace Fling

#endif  // WITH_EDITOR