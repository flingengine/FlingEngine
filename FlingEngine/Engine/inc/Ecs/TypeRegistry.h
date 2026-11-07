#pragma once

#include "Ecs/Component.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/Reflection.h"
#include "Json.h"

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Fling
{
    /**
     * Every component type that one world knows: an immutable copy of the
     * descriptions in a ModuleRegistration. A world builds its own in Init, and a
     * test builds one from its own ModuleRegistration. There is no global registry.
     */
    class TypeRegistry
    {
    public:
        TypeRegistry() = default;

        /**
         * Copy the types out of a ModuleRegistration. Logs a warning for a saved type
         * whose reflected fields cover much less than the struct (unless it is Partial).
         */
        static TypeRegistry Build(const ModuleRegistration& modules);

        /** The type with this saved name or former name. Null if there is none. */
        const TypeDesc* Find(std::string_view name) const;

        const TypeDesc* Find(TypeHash hash) const;

        template <::Fling::Component T>
        const TypeDesc* Find() const
        {
            return Find(TypeHashOf<T>);
        }

        /** A deleted type with this name. Null if there is none. */
        const RemovedComponentDesc* FindRemoved(std::string_view name) const;

        const std::vector<TypeDesc>& All() const { return m_Types; }

        bool Empty() const { return m_Types.empty(); }

    private:
        std::vector<TypeDesc> m_Types;
        std::vector<RemovedComponentDesc> m_Removed;
        std::unordered_map<std::string, size_t> m_ByName;
        std::unordered_map<TypeHash, size_t> m_ByHash;
    };

    /** What a save needs beyond the object: how to name another entity in the file. */
    struct SaveContext
    {
        /** For Entity fields: the storage that holds the targets' EntityGuid. Null saves every reference as 0. */
        const EntityStorage* Storage = nullptr;
    };

    /** What a load needs beyond the object: how to find an entity from a saved EntityGuid. */
    struct LoadContext
    {
        /** For Entity fields. Null, or a guid that is not in the map, loads Entity::Null. */
        const std::unordered_map<uint64, Entity>* GuidToEntity = nullptr;

        /** The file name, for warnings. */
        std::string SourceName;
    };

    /**
     * Write one object of the type to out (a JSON object): "$v" when the version is
     * above 1, then every saved field. A custom Serialize writes the object instead.
     */
    void SaveObject(const TypeDesc& type, const void* object, Json& out, const SaveContext& context);

    /**
     * Read one object of the type from in. Runs the upgrades from the saved "$v" up to
     * the current version first, then loads each field by name (or former name). A
     * missing field keeps its value. Unknown keys log a warning, except removed ones.
     * Runs AfterLoad last.
     */
    void LoadObject(const TypeDesc& type, void* object, const Json& in, const LoadContext& context);
} // namespace Fling
