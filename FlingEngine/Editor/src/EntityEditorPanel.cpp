#include "pch.h"
#include "EntityEditorPanel.h"

#include "ComponentTypeRegistry.h"
#include "Components/Name.hpp"
#include "EditableComponent.h"

#include <imgui.h>

#include <cstring>
#include <vector>

namespace Fling
{
	namespace
	{
		// NameComponent gets a dedicated rename field up top instead of showing up in
		// the generic component list, so it isn't offered twice.
		bool IsNameComponent(const ComponentTypeInfo& info)
		{
			return info.name && std::strcmp(info.name, "NameComponent") == 0;
		}
	}

	void EntityEditorPanel::Draw(entt::registry& t_Reg)
	{
		if (!m_ShowWindow)
		{
			return;
		}

		const entt::entity t_Entity = m_TargetEntity;

		if (ImGui::Begin("Entity Editor", &m_ShowWindow))
		{
			ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowWidth(), 30.0f), ImGuiCond_FirstUseEver);

			ImGui::TextUnformatted("editing:");
			ImGui::SameLine();

			if (t_Entity != entt::null && t_Reg.valid(t_Entity))
			{
				ImGui::Text("id: %u, v: %u", static_cast<uint32>(t_Reg.entity(t_Entity)), static_cast<uint32>(t_Reg.version(t_Entity)));
			}
			else
			{
				ImGui::TextUnformatted("no entity selected");
				ImGui::End();
				return;
			}

			// Rename field. Only touches the registry once the user actually edits it,
			// so just opening the editor doesn't silently attach a NameComponent.
			{
				char NameBuf[128] = {};
				const std::string& CurrentName = t_Reg.has<NameComponent>(t_Entity) ? t_Reg.get<NameComponent>(t_Entity).Name : std::string();
				std::strncpy(NameBuf, CurrentName.c_str(), sizeof(NameBuf) - 1);

				if (ImGui::InputText("Name", NameBuf, sizeof(NameBuf)))
				{
					if (!t_Reg.has<NameComponent>(t_Entity))
					{
						t_Reg.assign<NameComponent>(t_Entity);
					}
					t_Reg.get<NameComponent>(t_Entity).Name = NameBuf;
				}
			}

			ImGui::Separator();

			const std::vector<ComponentTypeInfo>& types = ComponentTypeRegistry::Get().All();

			std::vector<const ComponentTypeInfo*> missing;
			for (const ComponentTypeInfo& info : types)
			{
				if (IsNameComponent(info))
				{
					continue;
				}

				if (!info.has || !info.has(t_Reg, t_Entity))
				{
					missing.push_back(&info);
					continue;
				}

				ImGui::PushID(info.name);

				if (info.destroy)
				{
					if (ImGui::Button("-"))
					{
						info.destroy(t_Reg, t_Entity);
						ImGui::PopID();
						continue;	// Early out to avoid touching the just-removed component
					}
					ImGui::SameLine();
				}

				if (ImGui::CollapsingHeader(info.name))
				{
					ImGui::Indent(30.f);

					if (info.drawEditorWidget)
					{
						info.drawEditorWidget(t_Reg, t_Entity);
					}
					else
					{
						ImGui::TextDisabled("missing widget to display component!");
					}

					ImGui::Unindent(30.f);
				}

				ImGui::PopID();
			}

			if (!missing.empty())
			{
				if (ImGui::Button("+ Add Component"))
				{
					ImGui::OpenPopup("add component");
				}

				if (ImGui::BeginPopup("add component"))
				{
					ImGui::TextUnformatted("available:");
					ImGui::Separator();

					for (const ComponentTypeInfo* info : missing)
					{
						if (info->create && ImGui::Selectable(info->name))
						{
							info->create(t_Reg, t_Entity);
						}
					}

					ImGui::EndPopup();
				}
			}

			if (!t_Reg.has<EditableComponent>(t_Entity))
			{
				t_Reg.assign<EditableComponent>(t_Entity);
			}
		}
		ImGui::End();
	}
}	// namespace Fling
