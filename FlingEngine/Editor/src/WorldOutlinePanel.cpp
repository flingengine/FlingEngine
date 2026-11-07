#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "FlingTypes.h"
#include "HierarchyUtil.h"
#include "Logger.h"
#include "Strings/String.h"
#include "WorldOutlinePanel.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace Fling
{
	namespace
	{
		String LabelOf(const Fling::EntityStorage& t_Reg, const Fling::Entity t_Entity)
		{
			const NameComponent* Label = t_Reg.TryGet<NameComponent>(t_Entity);
			if (Label && !Label->Name.IsNone())
			{
				return String(Label->Name.ToView());
			}
			return "Entity " + std::to_string(t_Entity.Index);
		}
	} // namespace

	void WorldOutlinePanel::DrawNode(Fling::EntityStorage& t_Reg, const Fling::Entity t_Entity)
	{
		const Children* Kids = t_Reg.TryGet<Children>(t_Entity);
		const bool bHasChildren = Kids && !Kids->Value.empty();

		ImGuiTreeNodeFlags Flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!bHasChildren)
		{
			Flags |= ImGuiTreeNodeFlags_Leaf;
		}
		if (m_SelectedEntity == t_Entity)
		{
			Flags |= ImGuiTreeNodeFlags_Selected;
		}

		ImGui::PushID(static_cast<int>(t_Entity.Index));

		if (ImGui::SmallButton(" - "))
		{
			m_EntityToDestroy = t_Entity;
		}
		ImGui::SameLine();

		const String Label = LabelOf(t_Reg, t_Entity);
		const bool bOpen = ImGui::TreeNodeEx(Label.c_str(), Flags);
		if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen())
		{
			m_SelectedEntity = t_Entity;
		}

		// Drag an entity onto another to make it a child. Drop on the empty area to detach.
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("FLING_ENTITY", &t_Entity, sizeof(t_Entity));
			ImGui::TextUnformatted(Label.c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("FLING_ENTITY"))
			{
				m_PendingParent = { *static_cast<const Fling::Entity*>(Payload->Data), t_Entity };
			}
			ImGui::EndDragDropTarget();
		}

		if (bOpen)
		{
			if (bHasChildren)
			{
				// Copy, because a child can be changed while it is drawn
				const std::vector<Fling::Entity> ChildList = Kids->Value;
				for (const Fling::Entity Child : ChildList)
				{
					if (t_Reg.IsValid(Child))
					{
						DrawNode(t_Reg, Child);
					}
				}
			}
			ImGui::TreePop();
		}

		ImGui::PopID();
	}

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

			m_EntityToDestroy = Fling::Entity::Null;
			m_PendingParent = {};

			// Roots only. DrawNode draws each subtree from Children.
			std::vector<Fling::Entity> Roots;
			t_Reg.EachEntity([&](const Fling::Entity t_Entity)
			{
				if (HierarchyUtil::GetParent(t_Reg, t_Entity).IsNull())
				{
					Roots.push_back(t_Entity);
				}
			});
			for (const Fling::Entity Root : Roots)
			{
				DrawNode(t_Reg, Root);
			}

			// The rest of the window is a drop target that detaches
			ImGui::Dummy(ImGui::GetContentRegionAvail());
			if (ImGui::BeginDragDropTarget())
			{
				if (const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload("FLING_ENTITY"))
				{
					m_PendingParent = { *static_cast<const Fling::Entity*>(Payload->Data), Fling::Entity::Null };
				}
				ImGui::EndDragDropTarget();
			}

			if (!m_PendingParent.first.IsNull())
			{
				HierarchyUtil::SetParent(t_Reg, m_PendingParent.first, m_PendingParent.second);
			}

			if (!m_EntityToDestroy.IsNull())
			{
				F_LOG_TRACE("Delete {} and its children", LabelOf(t_Reg, m_EntityToDestroy));
				if (HierarchyUtil::IsAncestorOf(t_Reg, m_EntityToDestroy, m_SelectedEntity))
				{
					m_SelectedEntity = Fling::Entity::Null;
				}
				HierarchyUtil::DestroySubtree(t_Reg, m_EntityToDestroy);
			}
		}
		ImGui::End();
	}
}	// namespace Fling
