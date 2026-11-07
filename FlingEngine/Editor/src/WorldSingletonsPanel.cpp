#include "Ecs/TypeRegistry.h"
#include "Json.h"
#include "Strings/String.h"
#include "World.h"
#include "WorldSingletonsPanel.h"

#include <imgui.h>
#include <string>
#include <vector>

namespace Fling
{
    namespace
    {
        const char* KindName(const ComponentKind t_Kind)
        {
            return t_Kind == ComponentKind::Queue ? "Queue" : "Singleton";
        }
    } // namespace

    void WorldSingletonsPanel::Draw(Fling::EntityStorage& t_Reg)
    {
        if (!m_ShowWindow)
        {
            return;
        }

        if (ImGui::Begin("World Singletons", &m_ShowWindow))
        {
            ImGui::SetWindowSize(ImVec2(480.0f, 560.0f), ImGuiCond_FirstUseEver);

            if (ImGui::CollapsingHeader("Singletons", ImGuiTreeNodeFlags_DefaultOpen))
            {
                bool bAny = false;
                static const std::vector<TypeDesc> NoTypes;
                const std::vector<TypeDesc>& AllTypes = m_World ? m_World->GetTypes().All() : NoTypes;
                for (const TypeDesc& Type : AllTypes)
                {
                    if (!Type.IsSingleton())
                    {
                        continue;
                    }
                    bAny = true;

                    const void* Object = Type.GetSingletonConst ? Type.GetSingletonConst(t_Reg) : nullptr;
                    const String Label = Type.Name + "  (" + KindName(Type.Kind) +
                                              (Type.Transient ? ", transient" : ", saved") +
                                              (Object ? ")" : ", not in the world)");

                    if (!Object || Type.Fields.empty())
                    {
                        ImGui::BulletText("%s", Label.c_str());
                        continue;
                    }

                    if (ImGui::TreeNode(Type.Name.c_str(), "%s", Label.c_str()))
                    {
                        Json Value = Json::Object();
                        SaveObject(Type, Object, Value, SaveContext{ &t_Reg });
                        ImGui::TextUnformatted(Value.Dump(2).c_str());
                        ImGui::TreePop();
                    }
                }
                if (!bAny)
                {
                    ImGui::TextDisabled("No singleton types are registered.");
                }
            }

            if (ImGui::CollapsingHeader("System schedule"))
            {
                const Schedule* CurrentSchedule = m_World ? m_World->GetSchedule() : nullptr;
                if (CurrentSchedule)
                {
                    ImGui::TextUnformatted(CurrentSchedule->Dump().c_str());
                }
                else
                {
                    ImGui::TextDisabled("The world has no schedule.");
                }
            }
        }
        ImGui::End();
    }
} // namespace Fling
