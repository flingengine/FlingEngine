#pragma once

#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/TypeHash.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "Json.h"
#include "Strings/Name.h"
#include "Strings/String.h"

#include <cstddef>
#include <functional>
#include <initializer_list>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fling
{
    /** The kind of value in a reflected field. Save, load, and the editor switch on it. */
    enum class FieldKind : uint8
    {
        Bool,
        Int32,
        UInt32,
        Int64,
        UInt64,
        Float,
        Double,
        String,
        /** A Fling::Name, saved as its text so files stay readable and load to the same hash. */
        Name,
        Vec2,
        Vec3,
        Vec4,
        Quat,
        /** Saved by name when the field has an enum table, and as a number otherwise. */
        Enum,
        /** A reference to another entity. Saved as the target's EntityGuid, and remapped on load. */
        Entity,
        /** A type with its own save and load functions (see FieldTraits). */
        Custom,
    };

    /**
     * How a field type is saved. Specialize this for a type of your own: set
     * `static constexpr FieldKind Kind = FieldKind::Custom;` and add
     * `static void Save(const T&, Json& object, const StringView key)` and
     * `static bool Load(T&, const Json& object, const StringView key)` (true if the key was found).
     */
    template <typename T, typename = void>
    struct FieldTraits
    {
    };

    namespace Detail
    {
        template <FieldKind K>
        struct FieldKindIs
        {
            static constexpr FieldKind Kind = K;
        };
    } // namespace Detail

    // clang-format off
    template <> struct FieldTraits<bool> : Detail::FieldKindIs<FieldKind::Bool> {};
    template <> struct FieldTraits<int32> : Detail::FieldKindIs<FieldKind::Int32> {};
    template <> struct FieldTraits<uint32> : Detail::FieldKindIs<FieldKind::UInt32> {};
    template <> struct FieldTraits<int64> : Detail::FieldKindIs<FieldKind::Int64> {};
    template <> struct FieldTraits<uint64> : Detail::FieldKindIs<FieldKind::UInt64> {};
    template <> struct FieldTraits<float> : Detail::FieldKindIs<FieldKind::Float> {};
    template <> struct FieldTraits<double> : Detail::FieldKindIs<FieldKind::Double> {};
    template <> struct FieldTraits<String> : Detail::FieldKindIs<FieldKind::String> {};
    template <> struct FieldTraits<Name> : Detail::FieldKindIs<FieldKind::Name> {};
    template <> struct FieldTraits<glm::vec2> : Detail::FieldKindIs<FieldKind::Vec2> {};
    template <> struct FieldTraits<glm::vec3> : Detail::FieldKindIs<FieldKind::Vec3> {};
    template <> struct FieldTraits<glm::vec4> : Detail::FieldKindIs<FieldKind::Vec4> {};
    template <> struct FieldTraits<glm::quat> : Detail::FieldKindIs<FieldKind::Quat> {};
    template <> struct FieldTraits<Entity> : Detail::FieldKindIs<FieldKind::Entity> {};
    template <typename E> struct FieldTraits<E, std::enable_if_t<std::is_enum_v<E>>> : Detail::FieldKindIs<FieldKind::Enum> {};
    // clang-format on

    template <typename T>
    concept ReflectableField = requires
    {
        FieldTraits<T>::Kind;
    };

    /** One name and value of an enum field. */
    struct EnumEntry
    {
        String Name;
        int64 Value = 0;
    };

    /** One reflected member of a component. */
    struct FieldDesc
    {
        /** The saved name. Level files use it, so do not change it. Use FormerNames to rename. */
        String Name;

        /** Names this field had before. Load tries them after Name. Save writes only Name. */
        std::vector<String> FormerNames;

        /** The byte offset of the member in the component. */
        size_t Offset = 0;

        /** sizeof the member. */
        size_t Size = 0;

        /** alignof the member's type. An alignas on the member can make its real alignment bigger. */
        size_t Align = 1;

        FieldKind Kind = FieldKind::Float;

        /** Enum fields: the names. Empty means the enum is saved as a number. */
        std::vector<EnumEntry> EnumEntries;

        /** Enum fields: true if the underlying type is signed. */
        bool EnumSigned = false;

        /** Editor range for number fields. Used only when HasRange is true. */
        float Min = 0.0f;
        float Max = 0.0f;
        bool HasRange = false;

        String Tooltip;

        /** Not shown in the editor. Still saved. */
        bool Hidden = false;

        /** Shown in the editor, but cannot be changed there. */
        bool ReadOnly = false;

        /** Not saved. Shown in the editor as read-only. */
        bool Transient = false;

        /** Vec3 and Vec4 fields: the editor shows a color picker. */
        bool Color = false;

        /** Custom fields: save and load the member. */
        void (*CustomSave)(const void* field, Json& object, const StringView key) = nullptr;
        bool (*CustomLoad)(void* field, const Json& object, const StringView key) = nullptr;

        template <typename T>
        T& Of(void* object) const
        {
            return *reinterpret_cast<T*>(static_cast<unsigned char*>(object) + Offset);
        }

        void* Address(void* object) const { return static_cast<unsigned char*>(object) + Offset; }

        const void* Address(const void* object) const { return static_cast<const unsigned char*>(object) + Offset; }
    };

    /**
     * The full description of a component type: its declaration, its fields, how it
     * is versioned, and how to reach it in an EntityStorage. Made by
     * ModuleRegistration::Component and kept in a TypeRegistry.
     */
    struct TypeDesc
    {
        String Name;

        /** Names this type had before. A level that uses one still loads. */
        std::vector<String> FormerNames;

        /** The C++ type name (TypeNameOf), to tell two types with one saved name apart. */
        StringView CppName;

        /** A component type that an entity must have to have this one (RequiredOf). */
        struct Required
        {
            TypeHash Hash = 0;
            /** The saved name, for messages. */
            const char* Name = "";
        };

        /** The types in this type's Requires list. TypeRegistry::WithRequired gives those that they require too. */
        std::vector<Required> Requires;

        TypeHash Hash = 0;
        ComponentKind Kind = ComponentKind::Table;
        ComponentDomain Domain = ComponentDomain::Simulation;

        /** Never saved to a level (proxies, caches, queues). Read-only in the editor. */
        bool Transient = false;

        /** The current version. Saved as "$v" when it is above 1. */
        uint32 Version = 1;

        /** The group in the editor's Add Component menu. */
        String Category;

        /** True if the type reflects only some of its members on purpose. */
        bool Partial = false;

        size_t Size = 0;

        /** alignof the struct. No member is aligned more, and padding at the end is smaller. */
        size_t Align = 1;

        std::vector<FieldDesc> Fields;

        /** Field names that were removed. Load ignores them without a warning. */
        std::vector<String> RemovedFields;

        /** Each upgrade rewrites the saved object of FromVersion into FromVersion + 1. */
        struct Upgrade
        {
            uint32 FromVersion = 1;
            std::function<void(Json&)> Fn;
        };
        std::vector<Upgrade> Upgrades;

        /** Escape hatch: save and load the whole object by hand. The fields are then only for the editor. */
        std::function<void(const void*, Json&)> CustomSave;
        std::function<void(void*, const Json&)> CustomLoad;

        /** Runs after a load and after an editor change, for derived data (a cached matrix, a loaded asset). */
        std::function<void(void*)> AfterLoad;

        // Storage functions. Table and Tag types use the entity functions, and
        // singletons and queues use the singleton functions. The others are null.

        /** Fills a new object before it goes into the storage. Null leaves the default. */
        using FillFn = const std::function<void(void*)>*;

        bool (*Has)(const EntityStorage&, Entity) = nullptr;
        void* (*Get)(EntityStorage&, Entity) = nullptr;
        const void* (*GetConst)(const EntityStorage&, Entity) = nullptr;
        /**
         * Make a default object, run fill on it, then add it to the entity (or replace
         * the one it has). The add hooks see the filled object.
         */
        void (*Emplace)(EntityStorage&, Entity, FillFn fill) = nullptr;
        void (*Remove)(EntityStorage&, Entity) = nullptr;

        void* (*GetSingleton)(EntityStorage&) = nullptr;
        const void* (*GetSingletonConst)(const EntityStorage&) = nullptr;
        /** Make a default singleton, run fill on it, then set it (replacing any that exists). */
        void (*EmplaceSingleton)(EntityStorage&, FillFn fill) = nullptr;

        bool IsSingleton() const { return Kind == ComponentKind::Singleton || Kind == ComponentKind::Queue; }

        bool IsTag() const { return Kind == ComponentKind::Tag; }

        /** True if a level or snapshot saves this type. */
        bool IsSaved() const { return !Transient; }

        /** The field with this name or a former name. Null if there is none. */
        const FieldDesc* FindField(const StringView name) const;
    };

    /**
     * The bytes of a type that no reflected field covers and that cannot be padding:
     * the members a TypeBuilder forgot. TypeRegistry::Build warns when it is not 0.
     */
    size_t UnreflectedBytes(const TypeDesc& type);

    /** Sets the options of one field. Returned by TypeBuilder::Field. */
    class FieldBuilder
    {
    public:
        explicit FieldBuilder(TypeDesc& type, size_t index) : m_Type(&type), m_Index(index) {}

        /** The editor range for a number field. */
        FieldBuilder& Range(float min, float max)
        {
            Desc().Min = min;
            Desc().Max = max;
            Desc().HasRange = true;
            return *this;
        }

        FieldBuilder& Tooltip(const char* text)
        {
            Desc().Tooltip = text;
            return *this;
        }

        FieldBuilder& Hidden()
        {
            Desc().Hidden = true;
            return *this;
        }

        FieldBuilder& ReadOnly()
        {
            Desc().ReadOnly = true;
            return *this;
        }

        /** A Vec3 or Vec4 that is a color. The editor shows a color picker. */
        FieldBuilder& Color()
        {
            Desc().Color = true;
            return *this;
        }

        /** Not saved. Shown read-only in the editor. */
        FieldBuilder& Transient()
        {
            Desc().Transient = true;
            return *this;
        }

        /** Names the field had before. Load tries them after the current name. */
        FieldBuilder& FormerNames(std::initializer_list<const char*> names)
        {
            for (const char* name : names)
            {
                Desc().FormerNames.emplace_back(name);
            }
            return *this;
        }

        /** The saved name of each enum value. A field with a table saves names, so a reorder is safe. */
        template <typename E>
        FieldBuilder& Enum(std::initializer_list<std::pair<const char*, E>> entries)
        {
            static_assert(std::is_enum_v<E>, "Enum takes the values of an enum type.");
            for (const auto& [name, value] : entries)
            {
                Desc().EnumEntries.push_back(EnumEntry{name, static_cast<int64>(value)});
            }
            return *this;
        }

    private:
        FieldDesc& Desc() { return m_Type->Fields[m_Index]; }

        TypeDesc* m_Type;
        size_t m_Index;
    };

    /** Describes the fields and options of a component T. ModuleRegistration::Component passes it to your builder. */
    template <typename T>
    class TypeBuilder
    {
    public:
        explicit TypeBuilder(TypeDesc& desc) : m_Desc(&desc) {}

        /** The current version. Raise it with an Upgrade when the meaning or shape of the saved data changes. */
        TypeBuilder& Version(const uint32 version)
        {
            m_Desc->Version = version;
            return *this;
        }

        TypeBuilder& Category(const char* category)
        {
            m_Desc->Category = category;
            return *this;
        }

        /** The type reflects only some of its members on purpose (no warning for the others). */
        TypeBuilder& Partial()
        {
            m_Desc->Partial = true;
            return *this;
        }

        /** Names this component had before. A level that uses one still loads. */
        TypeBuilder& FormerNames(std::initializer_list<const char*> names)
        {
            for (const char* name : names)
            {
                m_Desc->FormerNames.emplace_back(name);
            }
            return *this;
        }

        /** A field. The member must belong to T, or the call does not compile. */
        template <typename M>
        FieldBuilder Field(const char* name, M T::*member)
        {
            static_assert(ReflectableField<M>,
                          "No FieldTraits for this member type. Specialize Fling::FieldTraits, or use Serialize.");
            FieldDesc field;
            field.Name = name;
            field.Offset = OffsetOf(member);
            field.Size = sizeof(M);
            field.Align = alignof(M);
            field.Kind = FieldTraits<M>::Kind;
            if constexpr (std::is_enum_v<M>)
            {
                static_assert(sizeof(M) <= sizeof(int64), "The enum is too large.");
                field.EnumSigned = std::is_signed_v<std::underlying_type_t<M>>;
            }
            if constexpr (FieldTraits<M>::Kind == FieldKind::Custom)
            {
                field.CustomSave = [](const void* value, Json& object, const StringView key)
                {
                    FieldTraits<M>::Save(*static_cast<const M*>(value), object, key);
                };
                field.CustomLoad = [](void* value, const Json& object, const StringView key) -> bool
                {
                    return FieldTraits<M>::Load(*static_cast<M*>(value), object, key);
                };
            }
            m_Desc->Fields.push_back(std::move(field));
            return FieldBuilder(*m_Desc, m_Desc->Fields.size() - 1);
        }

        /** A field name that was removed. Load ignores it without a warning, and nobody can use the name again. */
        TypeBuilder& Removed(const char* name)
        {
            m_Desc->RemovedFields.emplace_back(name);
            return *this;
        }

        /**
         * Rewrite a saved object of fromVersion into fromVersion + 1, before the fields
         * load. Write each upgrade once and do not change it.
         */
        TypeBuilder& Upgrade(const uint32 fromVersion, std::function<void(Json&)> fn)
        {
            m_Desc->Upgrades.push_back(TypeDesc::Upgrade{fromVersion, std::move(fn)});
            return *this;
        }

        /** Escape hatch: save and load the whole object by hand. Upgrades still run first. */
        TypeBuilder& Serialize(std::function<void(const T&, Json&)> save, std::function<void(T&, const Json&)> load)
        {
            m_Desc->CustomSave = [save = std::move(save)](const void* object, Json& out)
            {
                save(*static_cast<const T*>(object), out);
            };
            m_Desc->CustomLoad = [load = std::move(load)](void* object, const Json& in)
            {
                load(*static_cast<T*>(object), in);
            };
            return *this;
        }

        /** Runs after each load and each editor change, for derived data. */
        TypeBuilder& AfterLoad(std::function<void(T&)> fn)
        {
            m_Desc->AfterLoad = [fn = std::move(fn)](void* object)
            {
                fn(*static_cast<T*>(object));
            };
            return *this;
        }

    private:
        template <typename M>
        static size_t OffsetOf(M T::*member)
        {
            // No T is constructed. The address is only used to measure the offset.
            alignas(T) static unsigned char storage[sizeof(T)];
            const T* object = reinterpret_cast<const T*>(storage);
            return static_cast<size_t>(reinterpret_cast<const unsigned char*>(&(object->*member)) - storage);
        }

        TypeDesc* m_Desc;
    };
} // namespace Fling
