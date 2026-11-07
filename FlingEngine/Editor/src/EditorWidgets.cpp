#include "EditorWidgets.h"

#include <algorithm>
#include <cstring>
#include <imgui.h>
#include <string>

namespace Fling
{
    namespace
    {
        bool DrawFloat(const FieldDesc& field, float* value)
        {
            if (field.HasRange)
            {
                return ImGui::SliderFloat(field.Name.c_str(), value, field.Min, field.Max);
            }
            return ImGui::DragFloat(field.Name.c_str(), value, 0.05f);
        }

        bool DrawString(const FieldDesc& field, std::string* value)
        {
            char buffer[256] = {};
            std::strncpy(buffer, value->c_str(), sizeof(buffer) - 1);
            if (ImGui::InputText(field.Name.c_str(), buffer, sizeof(buffer)))
            {
                *value = buffer;
                return true;
            }
            return false;
        }

        int64 ReadEnum(const FieldDesc& field, const void* address)
        {
            int64 value = 0;
            std::memcpy(&value, address, std::min<size_t>(field.Size, sizeof(value)));
            if (field.EnumSigned && field.Size < sizeof(value))
            {
                const int shift = static_cast<int>((sizeof(value) - field.Size) * 8);
                value = static_cast<int64>(static_cast<uint64>(value) << shift) >> shift;
            }
            return value;
        }

        bool DrawEnum(const FieldDesc& field, void* address)
        {
            const int64 current = ReadEnum(field, address);
            if (field.EnumEntries.empty())
            {
                int asInt = static_cast<int>(current);
                if (ImGui::InputInt(field.Name.c_str(), &asInt))
                {
                    const int64 value = asInt;
                    std::memcpy(address, &value, std::min<size_t>(field.Size, sizeof(value)));
                    return true;
                }
                return false;
            }

            const auto selected = std::find_if(field.EnumEntries.begin(),
                                               field.EnumEntries.end(),
                                               [&](const EnumEntry& e) { return e.Value == current; });
            const char* preview = selected != field.EnumEntries.end() ? selected->Name.c_str() : "?";
            bool changed = false;
            if (ImGui::BeginCombo(field.Name.c_str(), preview))
            {
                for (const EnumEntry& entry : field.EnumEntries)
                {
                    if (ImGui::Selectable(entry.Name.c_str(), entry.Value == current))
                    {
                        std::memcpy(address, &entry.Value, std::min<size_t>(field.Size, sizeof(entry.Value)));
                        changed = true;
                    }
                }
                ImGui::EndCombo();
            }
            return changed;
        }

        bool DrawField(const FieldDesc& field, void* object)
        {
            void* address = field.Address(object);
            const char* label = field.Name.c_str();
            switch (field.Kind)
            {
            case FieldKind::Bool:
                return ImGui::Checkbox(label, static_cast<bool*>(address));
            case FieldKind::Int32:
                return ImGui::InputScalar(label, ImGuiDataType_S32, address);
            case FieldKind::UInt32:
                return ImGui::InputScalar(label, ImGuiDataType_U32, address);
            case FieldKind::Int64:
                return ImGui::InputScalar(label, ImGuiDataType_S64, address);
            case FieldKind::UInt64:
                return ImGui::InputScalar(label, ImGuiDataType_U64, address);
            case FieldKind::Float:
                return DrawFloat(field, static_cast<float*>(address));
            case FieldKind::Double:
                return ImGui::InputScalar(label, ImGuiDataType_Double, address);
            case FieldKind::String:
                return DrawString(field, static_cast<std::string*>(address));
            case FieldKind::Vec2:
                return ImGui::DragFloat2(label, static_cast<float*>(address), 0.05f);
            case FieldKind::Vec3:
                return field.Color ? ImGui::ColorEdit3(label, static_cast<float*>(address))
                                   : ImGui::DragFloat3(label, static_cast<float*>(address), 0.05f);
            case FieldKind::Vec4:
                return field.Color ? ImGui::ColorEdit4(label, static_cast<float*>(address))
                                   : ImGui::DragFloat4(label, static_cast<float*>(address), 0.05f);
            case FieldKind::Quat:
                return ImGui::DragFloat4(label, static_cast<float*>(address), 0.01f);
            case FieldKind::Enum:
                return DrawEnum(field, address);
            case FieldKind::Entity:
            {
                const Entity target = *static_cast<const Entity*>(address);
                if (target.IsNull())
                {
                    ImGui::LabelText(label, "none");
                }
                else
                {
                    ImGui::LabelText(label, "entity %u (v%u)", target.Index, target.Generation);
                }
                return false;
            }
            case FieldKind::Custom:
                ImGui::LabelText(label, "(no editor widget)");
                return false;
            }
            return false;
        }
    } // namespace

    const EditorWidgets::DrawFn* EditorWidgets::Find(const TypeHash hash) const
    {
        const auto found = m_Overrides.find(hash);
        return found == m_Overrides.end() ? nullptr : &found->second;
    }

    void EditorWidgets::Draw(const TypeDesc& type, EntityStorage& storage, const Entity entity) const
    {
        if (const DrawFn* custom = Find(type.Hash))
        {
            (*custom)(storage, entity);
            return;
        }

        if (type.IsTag())
        {
            ImGui::TextDisabled("tag (no data)");
            return;
        }

        void* object = type.Get ? type.Get(storage, entity) : nullptr;
        if (object == nullptr)
        {
            return;
        }
        if (type.Fields.empty())
        {
            ImGui::TextDisabled(type.Transient ? "transient (not saved, no fields)" : "no reflected fields");
            return;
        }
        if (DrawReflected(type, object, type.Transient) && type.AfterLoad)
        {
            type.AfterLoad(object);
        }
    }

    bool EditorWidgets::DrawReflected(const TypeDesc& type, void* object, const bool readOnly)
    {
        bool changed = false;
        for (const FieldDesc& field : type.Fields)
        {
            if (field.Hidden)
            {
                continue;
            }
            ImGui::PushID(field.Name.c_str());
            const bool locked = readOnly || field.ReadOnly || field.Transient;
            ImGui::BeginDisabled(locked);
            const bool fieldChanged = DrawField(field, object);
            ImGui::EndDisabled();
            changed |= fieldChanged && !locked;
            if (!field.Tooltip.empty() && ImGui::IsItemHovered())
            {
                ImGui::SetTooltip("%s", field.Tooltip.c_str());
            }
            ImGui::PopID();
        }
        return changed;
    }
} // namespace Fling
