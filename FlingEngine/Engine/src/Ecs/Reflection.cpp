#include "Components/EntityGuid.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Reflection.h"
#include "Ecs/TypeRegistry.h"
#include "Logger.h"
#include "MathUtils.h"
#include "Strings/String.h"

#include <algorithm>
#include <cstring>

namespace Fling
{
    namespace
    {
        constexpr const char* VersionKey = "$v";

        int64 ReadEnumValue(const FieldDesc& field, const void* address)
        {
            switch (field.Size)
            {
            case 1:
                return field.EnumSigned ? *static_cast<const int8*>(address) : *static_cast<const uint8*>(address);
            case 2:
                return field.EnumSigned ? *static_cast<const int16*>(address) : *static_cast<const uint16*>(address);
            case 4:
                return field.EnumSigned ? *static_cast<const int32*>(address) : *static_cast<const uint32*>(address);
            default:
                return *static_cast<const int64*>(address);
            }
        }

        void WriteEnumValue(const FieldDesc& field, void* address, const int64 value)
        {
            switch (field.Size)
            {
            case 1:
            {
                const uint8 v = static_cast<uint8>(value);
                std::memcpy(address, &v, 1);
                break;
            }
            case 2:
            {
                const uint16 v = static_cast<uint16>(value);
                std::memcpy(address, &v, 2);
                break;
            }
            case 4:
            {
                const uint32 v = static_cast<uint32>(value);
                std::memcpy(address, &v, 4);
                break;
            }
            default:
                std::memcpy(address, &value, 8);
                break;
            }
        }

        uint64 GuidOf(const SaveContext& context, const Entity target)
        {
            if (context.Storage == nullptr || !context.Storage->IsValid(target))
            {
                return 0;
            }
            const EntityGuid* guid = context.Storage->TryGet<EntityGuid>(target);
            return guid ? guid->Value : 0;
        }

        void SaveField(const FieldDesc& field, const void* object, Json& out, const SaveContext& context)
        {
            const void* address = field.Address(object);
            const StringView key = field.Name;
            switch (field.Kind)
            {
            case FieldKind::Bool:
                out.Set(key, *static_cast<const bool*>(address));
                break;
            case FieldKind::Int32:
                out.Set(key, static_cast<int>(*static_cast<const int32*>(address)));
                break;
            case FieldKind::UInt32:
                out.SetUInt64(key, *static_cast<const uint32*>(address));
                break;
            case FieldKind::Int64:
                out.SetInt64(key, *static_cast<const int64*>(address));
                break;
            case FieldKind::UInt64:
                out.SetUInt64(key, *static_cast<const uint64*>(address));
                break;
            case FieldKind::Float:
                out.Set(key, *static_cast<const float*>(address));
                break;
            case FieldKind::Double:
                out.SetDouble(key, *static_cast<const double*>(address));
                break;
            case FieldKind::String:
                out.Set(key, *static_cast<const String*>(address));
                break;
            case FieldKind::Name:
                out.Set(key, static_cast<const Name*>(address)->ToView());
                break;
            case FieldKind::Vec2:
                out.Set(key, *static_cast<const glm::vec2*>(address));
                break;
            case FieldKind::Vec3:
                out.Set(key, *static_cast<const glm::vec3*>(address));
                break;
            case FieldKind::Vec4:
                out.Set(key, *static_cast<const glm::vec4*>(address));
                break;
            case FieldKind::Quat:
                out.SetQuat(key, *static_cast<const glm::quat*>(address));
                break;
            case FieldKind::Enum:
            {
                const int64 value = ReadEnumValue(field, address);
                const auto entry = std::find_if(field.EnumEntries.begin(),
                                                field.EnumEntries.end(),
                                                [&](const EnumEntry& e) { return e.Value == value; });
                if (entry != field.EnumEntries.end())
                {
                    out.Set(key, entry->Name);
                }
                else
                {
                    out.SetInt64(key, value);
                }
                break;
            }
            case FieldKind::Entity:
                out.SetUInt64(key, GuidOf(context, *static_cast<const Entity*>(address)));
                break;
            case FieldKind::Custom:
                if (field.CustomSave)
                {
                    field.CustomSave(address, out, key);
                }
                break;
            }
        }

        /** Load one field from the given key. False if the key is not there. */
        bool LoadFieldFrom(
            const FieldDesc& field, void* object, const Json& in, const StringView key, const LoadContext& context)
        {
            if (!in.Contains(key))
            {
                return false;
            }
            void* address = field.Address(object);
            switch (field.Kind)
            {
            case FieldKind::Bool:
                field.Of<bool>(object) = in.GetBool(key, field.Of<bool>(object));
                break;
            case FieldKind::Int32:
                field.Of<int32>(object) = static_cast<int32>(in.GetInt64(key, field.Of<int32>(object)));
                break;
            case FieldKind::UInt32:
                field.Of<uint32>(object) = static_cast<uint32>(in.GetUInt64(key, field.Of<uint32>(object)));
                break;
            case FieldKind::Int64:
                field.Of<int64>(object) = in.GetInt64(key, field.Of<int64>(object));
                break;
            case FieldKind::UInt64:
                field.Of<uint64>(object) = in.GetUInt64(key, field.Of<uint64>(object));
                break;
            case FieldKind::Float:
                field.Of<float>(object) = in.GetFloat(key, field.Of<float>(object));
                break;
            case FieldKind::Double:
                field.Of<double>(object) = in.GetDouble(key, field.Of<double>(object));
                break;
            case FieldKind::String:
                field.Of<String>(object) = in.GetString(key, field.Of<String>(object));
                break;
            case FieldKind::Name:
                // Loading makes the same hash from the text on every machine
                field.Of<Name>(object) = Name(in.GetString(key, field.Of<Name>(object).ToView()));
                break;
            case FieldKind::Vec2:
                field.Of<glm::vec2>(object) = in.GetVec2(key, field.Of<glm::vec2>(object));
                break;
            case FieldKind::Vec3:
                field.Of<glm::vec3>(object) = in.GetVec3(key, field.Of<glm::vec3>(object));
                break;
            case FieldKind::Vec4:
                field.Of<glm::vec4>(object) = in.GetVec4(key, field.Of<glm::vec4>(object));
                break;
            case FieldKind::Quat:
                field.Of<glm::quat>(object) = in.GetQuat(key, field.Of<glm::quat>(object));
                break;
            case FieldKind::Enum:
            {
                if (in.IsString(key))
                {
                    const String name = in.GetString(key);
                    const auto entry = std::find_if(field.EnumEntries.begin(),
                                                    field.EnumEntries.end(),
                                                    [&](const EnumEntry& e) { return e.Name == name; });
                    if (entry != field.EnumEntries.end())
                    {
                        WriteEnumValue(field, address, entry->Value);
                    }
                    else
                    {
                        F_LOG_WARN("{}: unknown value '{}' for field '{}'. It keeps its default.",
                                   context.SourceName,
                                   name,
                                   field.Name);
                    }
                }
                else
                {
                    WriteEnumValue(field, address, in.GetInt64(key, ReadEnumValue(field, address)));
                }
                break;
            }
            case FieldKind::Entity:
            {
                const uint64 guid = in.GetUInt64(key, 0);
                Entity target = Entity::Null;
                if (guid != 0 && context.GuidToEntity != nullptr)
                {
                    const auto found = context.GuidToEntity->find(guid);
                    if (found != context.GuidToEntity->end())
                    {
                        target = found->second;
                    }
                }
                field.Of<Entity>(object) = target;
                break;
            }
            case FieldKind::Custom:
                return field.CustomLoad ? field.CustomLoad(address, in, key) : false;
            }
            return true;
        }

        bool IsKnownKey(const TypeDesc& type, const StringView key)
        {
            if (key == VersionKey || type.FindField(key) != nullptr)
            {
                return true;
            }
            return std::find(type.RemovedFields.begin(), type.RemovedFields.end(), key) != type.RemovedFields.end();
        }
    } // namespace

    /**
     * The bytes of the struct that no reflected field covers and that cannot be
     * padding. A gap before a field is padding only if some alignment A, with
     * gap < A <= the struct's alignment, puts the field at its offset. (A member's
     * alignment, alignas included, is never more than the struct's.) A gap at the
     * end is padding only if it is smaller than the struct's alignment. Anything
     * else is a member that was not reflected. Exact on every compiler, whatever
     * the alignment of the math types.
     */
    size_t UnreflectedBytes(const TypeDesc& type)
    {
        std::vector<const FieldDesc*> byOffset;
        for (const FieldDesc& field : type.Fields)
        {
            byOffset.push_back(&field);
        }
        std::sort(byOffset.begin(),
                  byOffset.end(),
                  [](const FieldDesc* a, const FieldDesc* b) { return a->Offset < b->Offset; });

        size_t missing = 0;
        size_t covered = 0;
        for (const FieldDesc* field : byOffset)
        {
            if (field->Offset > covered)
            {
                const size_t gap = field->Offset - covered;
                bool isPadding = false;
                for (size_t align = 1; align <= type.Align; align *= 2)
                {
                    isPadding = isPadding || (align > gap && field->Offset % align == 0);
                }
                missing += isPadding ? 0 : gap;
            }
            covered = Math::Max(covered, field->Offset + field->Size);
        }
        if (type.Size > covered && type.Size - covered >= type.Align)
        {
            missing += type.Size - covered;
        }
        return missing;
    }

    const FieldDesc* TypeDesc::FindField(const StringView name) const
    {
        for (const FieldDesc& field : Fields)
        {
            if (field.Name == name)
            {
                return &field;
            }
        }
        for (const FieldDesc& field : Fields)
        {
            if (std::find(field.FormerNames.begin(), field.FormerNames.end(), name) != field.FormerNames.end())
            {
                return &field;
            }
        }
        return nullptr;
    }

    void ModuleRegistration::AddType(TypeDesc desc)
    {
        const auto taken = std::find_if(
            m_Types.begin(), m_Types.end(), [&](const TypeDesc& other) { return other.Name == desc.Name; });
        if (taken != m_Types.end())
        {
            F_LOG_WARN("Component type '{}' is registered twice. The second registration is ignored.", desc.Name);
            return;
        }
        m_Types.push_back(std::move(desc));
    }

    TypeRegistry TypeRegistry::Build(const ModuleRegistration& modules)
    {
        TypeRegistry registry;
        registry.m_Types = modules.Types();
        registry.m_Removed = modules.RemovedComponents();

        for (size_t i = 0; i < registry.m_Types.size(); ++i)
        {
            const TypeDesc& type = registry.m_Types[i];
            registry.m_ByName.emplace(type.Name, i);
            registry.m_ByHash.emplace(type.Hash, i);

            if (!type.Transient && !type.Partial && !type.CustomSave && !type.IsTag() && type.Size > 0)
            {
                const size_t Missing = UnreflectedBytes(type);
                if (Missing > 0)
                {
                    F_LOG_WARN("Component '{}' has {} bytes that are not reflected fields or padding. Reflect "
                               "the missing fields, or call Partial() if they are not saved on purpose.",
                               type.Name,
                               Missing);
                }
            }
        }

        // Former names come second, so a current name always wins.
        for (size_t i = 0; i < registry.m_Types.size(); ++i)
        {
            for (const String& former : registry.m_Types[i].FormerNames)
            {
                registry.m_ByName.emplace(former, i);
            }
        }
        return registry;
    }

    const TypeDesc* TypeRegistry::Find(const StringView name) const
    {
        const auto found = m_ByName.find(String(name));
        return found == m_ByName.end() ? nullptr : &m_Types[found->second];
    }

    const TypeDesc* TypeRegistry::Find(const TypeHash hash) const
    {
        const auto found = m_ByHash.find(hash);
        return found == m_ByHash.end() ? nullptr : &m_Types[found->second];
    }

    const RemovedComponentDesc* TypeRegistry::FindRemoved(const StringView name) const
    {
        for (const RemovedComponentDesc& removed : m_Removed)
        {
            if (removed.Name == name)
            {
                return &removed;
            }
        }
        return nullptr;
    }

    void SaveObject(const TypeDesc& type, const void* object, Json& out, const SaveContext& context)
    {
        if (type.Version > 1)
        {
            out.SetInt64(VersionKey, type.Version);
        }

        if (type.CustomSave)
        {
            type.CustomSave(object, out);
            return;
        }

        for (const FieldDesc& field : type.Fields)
        {
            if (!field.Transient)
            {
                SaveField(field, object, out, context);
            }
        }
    }

    void LoadObject(const TypeDesc& type, void* object, const Json& in, const LoadContext& context)
    {
        const int64 savedVersion = in.GetInt64(VersionKey, 1);
        const Json* source = &in;
        Json upgraded;

        if (savedVersion > static_cast<int64>(type.Version))
        {
            F_LOG_WARN("{}: '{}' was saved at version {}, which is newer than this build ({}). Fields load by name.",
                       context.SourceName,
                       type.Name,
                       savedVersion,
                       type.Version);
        }
        else if (savedVersion < static_cast<int64>(type.Version))
        {
            upgraded = in;
            for (uint32 from = static_cast<uint32>(savedVersion); from < type.Version; ++from)
            {
                const auto step = std::find_if(type.Upgrades.begin(),
                                               type.Upgrades.end(),
                                               [&](const TypeDesc::Upgrade& u) { return u.FromVersion == from; });
                if (step == type.Upgrades.end())
                {
                    F_LOG_WARN("{}: '{}' has no upgrade from version {}. Fields load by name.",
                               context.SourceName,
                               type.Name,
                               from);
                    continue;
                }
                step->Fn(upgraded);
            }
            source = &upgraded;
        }

        if (type.CustomLoad)
        {
            type.CustomLoad(object, *source);
        }
        else
        {
            for (const FieldDesc& field : type.Fields)
            {
                if (field.Transient)
                {
                    continue;
                }
                if (LoadFieldFrom(field, object, *source, field.Name, context))
                {
                    continue;
                }
                for (const String& former : field.FormerNames)
                {
                    if (LoadFieldFrom(field, object, *source, former, context))
                    {
                        break;
                    }
                }
            }

            for (const StringView key : source->Keys())
            {
                if (!IsKnownKey(type, key))
                {
                    F_LOG_WARN(
                        "{}: '{}' has an unknown field '{}'. It is ignored.", context.SourceName, type.Name, key);
                }
            }
        }

        if (type.AfterLoad)
        {
            type.AfterLoad(object);
        }
    }
} // namespace Fling
