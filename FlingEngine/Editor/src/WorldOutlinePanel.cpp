#include "pch.h"
#include "WorldOutlinePanel.h"

#include "Components/Name.hpp"

#include <imgui.h>

#include <sstream>
#include <vector>

namespace Fling
{
	void WorldOutlinePanel::Draw(entt::registry& t_Reg)
	{
		if (!m_ShowWindow)
		{
			return;
		}

		if (ImGui::Begin("World Outline", &m_ShowWindow))
		{
			ImGui::SetWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
			ImGui::SetWindowPos(ImVec2(0.0f, 30.0f), ImGuiCond_FirstUseEver);

			if (ImGui::Button("+ Add Entity", ImVec2(ImGui::GetWindowWidth(), 0.f)))
			{
				m_SelectedEntity = t_Reg.create();
			}

			ImGui::Separator();

			std::vector<entt::entity> entities;
			t_Reg.each([&](entt::entity entity)
			{
				entities.push_back(entity);
			});

			entt::entity entityToDestroy = entt::null;
			for (entt::entity entity : entities)
			{
				if (!t_Reg.valid(entity))
				{
					continue;
				}

				const bool bIsSelected = (m_SelectedEntity == entity);

				std::string label;
				if (t_Reg.has<NameComponent>(entity) && !t_Reg.get<NameComponent>(entity).Name.empty())
				{
					label = t_Reg.get<NameComponent>(entity).Name;
				}
				else
				{
					std::ostringstream os;
					os << "Entity " << static_cast<uint64>(entity);
					label = os.str();
				}

				ImGui::PushID(static_cast<int>(entity));

				if (ImGui::Button(" - "))
				{
					F_LOG_TRACE("Delete {}", label);
					entityToDestroy = entity;
				}

				ImGui::SameLine();

				if (bIsSelected)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, (ImVec4)ImColor::HSV(7.0f, 0.6f, 0.6f));
				}

				if (ImGui::Button(label.c_str(), ImVec2(ImGui::GetWindowWidth(), 0.f)))
				{
					m_SelectedEntity = entity;
				}

				if (bIsSelected)
				{
					ImGui::PopStyleColor(1);
				}

				ImGui::PopID();
			}

			if (entityToDestroy != entt::null)
			{
				if (m_SelectedEntity == entityToDestroy)
				{
					m_SelectedEntity = entt::null;
				}
				t_Reg.destroy(entityToDestroy);
			}
		}
		ImGui::End();
	}
}	// namespace Fling
