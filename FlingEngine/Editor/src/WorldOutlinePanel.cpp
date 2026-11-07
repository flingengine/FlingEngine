#include "Components/Name.hpp"
#include "FlingTypes.h"
#include "Logger.h"
#include "WorldOutlinePanel.h"

#include <imgui.h>
#include <sstream>
#include <vector>

namespace Fling
{
	void WorldOutlinePanel::Draw(Fling::EntityStorage& t_Reg)
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
				m_SelectedEntity = t_Reg.Create();
			}

			ImGui::Separator();

			std::vector<Fling::Entity> entities;
			t_Reg.EachEntity([&](Fling::Entity entity)
			{
				entities.push_back(entity);
			});

			Fling::Entity entityToDestroy = Fling::Entity::Null;
			for (Fling::Entity entity : entities)
			{
				if (!t_Reg.IsValid(entity))
				{
					continue;
				}

				const bool bIsSelected = (m_SelectedEntity == entity);

				std::string label;
				if (t_Reg.Has<NameComponent>(entity) && !t_Reg.Get<NameComponent>(entity).Name.empty())
				{
					label = t_Reg.Get<NameComponent>(entity).Name;
				}
				else
				{
					std::ostringstream os;
					os << "Entity " << entity.Index;
					label = os.str();
				}

				ImGui::PushID(static_cast<int>(entity.Index));

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

			if (entityToDestroy != Fling::Entity::Null)
			{
				if (m_SelectedEntity == entityToDestroy)
				{
					m_SelectedEntity = Fling::Entity::Null;
				}
				t_Reg.Destroy(entityToDestroy);
			}
		}
		ImGui::End();
	}
}	// namespace Fling
