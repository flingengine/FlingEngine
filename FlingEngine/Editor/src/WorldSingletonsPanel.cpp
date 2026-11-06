#include "ComponentTypeRegistry.h"
#include "Json.h"
#include "World.h"
#include "WorldSingletonsPanel.h"

#include <imgui.h>
#include <string>

namespace Fling
{
    namespace
    {
        const char* KindName(const ComponentKind t_Kind)
        {
            return t_Kind == ComponentKind::Queue ? "Queue" : "Singleton";
        }
    } // namespace

    void WorldSingletonsPanel::Draw(entt::registry& t_Reg)
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
                for (const ComponentTypeInfo& Type : ComponentTypeRegistry::Get().All())
                {
                    if (!Type.IsSingleton())
                    {
                        continue;
                    }
                    bAny = true;

                    const bool bPresent = Type.hasSingleton && Type.hasSingleton(t_Reg);
                    const std::string Label = std::string(Type.name) + "  (" + KindName(Type.kind) +
                                              (Type.transient ? ", transient" : ", saved") +
                                              (bPresent ? ")" : ", not in the world)");

                    if (!bPresent || !Type.saveSingleton)
                    {
                        ImGui::BulletText("%s", Label.c_str());
                        continue;
                    }

                    if (ImGui::TreeNode(Type.name, "%s", Label.c_str()))
                    {
                        Json Value = Json::Object();
                        Type.saveSingleton(t_Reg, Value);
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
