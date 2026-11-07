#include "Components/Name.hpp"
#include "EditorWidgets.h"
#include "EntityEditorPanel.h"
#include "FlingTypes.h"
#include "Strings/String.h"
#include "World.h"

#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <string>
#include <vector>

namespace Fling
{
    namespace
    {
        // NameComponent gets a dedicated rename field up top instead of showing up in
        // the generic component list, so it isn't offered twice.
        bool IsNameComponent(const TypeDesc& t_Type)
        {
            return t_Type.Name == NameComponent::Component.Name;
        }
    } // namespace

    void EntityEditorPanel::Draw(Fling::EntityStorage& t_Reg)
    {
        if (!m_ShowWindow)
        {
            return;
        }

        const Fling::Entity t_Entity = m_TargetEntity;

        if (ImGui::Begin("Entity Editor", &m_ShowWindow))
        {
            ImGui::SetNextWindowSize(ImVec2(250.0f, 400.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(ImGui::GetWindowWidth(), 30.0f), ImGuiCond_FirstUseEver);

            ImGui::TextUnformatted("editing:");
            ImGui::SameLine();

            if (t_Entity != Fling::Entity::Null && t_Reg.IsValid(t_Entity))
            {
                ImGui::Text("id: %u, v: %u", t_Entity.Index, t_Entity.Generation);
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
                static Fling::Entity BufferEntity = Fling::Entity::Null;
                static bool bEditing = false;
                if (!bEditing || BufferEntity != t_Entity)
                {
                    const StringView CurrentName = t_Reg.Has<NameComponent>(t_Entity)
                                                       ? t_Reg.Get<NameComponent>(t_Entity).Name.ToView()
                                                       : StringView();
                    const size_t Length = std::min(CurrentName.size(), sizeof(NameBuf) - 1);
                    std::memcpy(NameBuf, CurrentName.data(), Length);
                    NameBuf[Length] = '\0';
                    BufferEntity = t_Entity;
                }

                ImGui::InputText("Name", NameBuf, sizeof(NameBuf));
                bEditing = ImGui::IsItemActive();
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    if (!t_Reg.Has<NameComponent>(t_Entity))
                    {
                        t_Reg.Add<NameComponent>(t_Entity);
                    }
                    t_Reg.Get<NameComponent>(t_Entity).Name = Name(StringView(NameBuf));
                }
            }

            ImGui::Separator();

            if (!m_World || !m_Widgets)
            {
                ImGui::TextDisabled("no world");
                ImGui::End();
                return;
            }

            std::vector<const TypeDesc*> missing;
            for (const TypeDesc& type : m_World->GetTypes().All())
            {
                // Singletons and queues belong to the world, not to an entity
                if (IsNameComponent(type) || type.IsSingleton())
                {
                    continue;
                }

                if (!type.Has || !type.Has(t_Reg, t_Entity))
                {
                    // Transient types are runtime data that systems add, not something to author
                    if (!type.Transient)
                    {
                        missing.push_back(&type);
                    }
                    continue;
                }

                ImGui::PushID(type.Name.c_str());

                if (type.Remove)
                {
                    if (ImGui::Button("-"))
                    {
                        type.Remove(t_Reg, t_Entity);
                        ImGui::PopID();
                        continue; // Early out to avoid touching the just-removed component
                    }
                    ImGui::SameLine();
                }

                const String Header = type.Transient ? type.Name + " (transient)" : type.Name;
                if (ImGui::CollapsingHeader(Header.c_str()))
                {
                    ImGui::Indent(30.f);
                    m_Widgets->Draw(type, t_Reg, t_Entity);
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

                    // Grouped by category, in name order inside each group
                    std::sort(missing.begin(),
                              missing.end(),
                              [](const TypeDesc* a, const TypeDesc* b)
                              { return a->Category != b->Category ? a->Category < b->Category : a->Name < b->Name; });
                    String currentCategory = "\x01";
                    for (const TypeDesc* type : missing)
                    {
                        if (type->Category != currentCategory)
                        {
                            currentCategory = type->Category;
                            ImGui::TextDisabled("%s", currentCategory.empty() ? "Other" : currentCategory.c_str());
                        }
                        if (type->Emplace && ImGui::Selectable(type->Name.c_str()))
                        {
                            type->Emplace(t_Reg, t_Entity, nullptr);
                        }
                    }

                    ImGui::EndPopup();
                }
            }
        }
        ImGui::End();
    }
} // namespace Fling
