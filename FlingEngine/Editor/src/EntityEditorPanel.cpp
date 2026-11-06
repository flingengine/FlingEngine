#include "Components/Name.hpp"
#include "ComponentTypeRegistry.h"
#include "EditableComponent.h"
#include "EntityEditorPanel.h"
#include "FlingTypes.h"

#include <cstring>
#include <imgui.h>
#include <vector>

namespace Fling
{
	namespace
	{
		// NameComponent gets a dedicated rename field up top instead of showing up in
		// the generic component list, so it isn't offered twice.
		bool IsNameComponent(const ComponentTypeInfo& info)
		{
			return info.TypeName == "NameComponent"_name;
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
			// The new Name is made only when the edit ends. A Name per key press would add
			// every partial text ("P", "Pl", ...) to the name table, which keeps them all.
			{
				static char NameBuf[128] = {};
				static entt::entity BufferEntity = entt::null;
				static bool bEditing = false;
				if (!bEditing || BufferEntity != t_Entity)
				{
					const StringView CurrentName = t_Reg.has<NameComponent>(t_Entity) ? t_Reg.get<NameComponent>(t_Entity).Name.ToView() : StringView();
					const size_t Length = std::min(CurrentName.size(), sizeof(NameBuf) - 1);
					std::memcpy(NameBuf, CurrentName.data(), Length);
					NameBuf[Length] = '\0';
					BufferEntity = t_Entity;
				}

				ImGui::InputText("Name", NameBuf, sizeof(NameBuf));
				bEditing = ImGui::IsItemActive();
				if (ImGui::IsItemDeactivatedAfterEdit())
				{
					if (!t_Reg.has<NameComponent>(t_Entity))
					{
						t_Reg.assign<NameComponent>(t_Entity);
					}
					t_Reg.get<NameComponent>(t_Entity).Name = Name(StringView(NameBuf));
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
