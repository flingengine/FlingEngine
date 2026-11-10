#pragma once

#include "Ecs/TypeHash.h"
#include "Ecs/TypeList.h"
#include "FlingTypes.h"
#include "Strings/String.h"
#include "TypeName.h"

#include <string_view>
#include <type_traits>

namespace Fling
{
    /** How a component type is stored, and how many there are. */
    enum class ComponentKind : uint8
    {
        /** One per entity. The default. */
        Table,
        /** An empty struct that marks an entity. It has no data. */
        Tag,
        /** Exactly one per world. It holds what would otherwise be system state. */
        Singleton,
        /** A singleton that holds deferred requests. Systems append to it, and one system consumes it. */
        Queue,
    };

    /** Which clock may write a component type. */
    enum class ComponentDomain : uint8
    {
        /** Written only at the fixed tick. The default. */
        Simulation,
        /** Written only at the frame rate (camera smoothing, UI state). The simulation never reads it. */
        Presentation,
    };

    /** Where the storage keeps a Table component (Plans/ecs-plan.md section 1.10). */
    enum class ComponentStorage : uint8
    {
        /** A column in the entity's archetype. Fastest to iterate. Adding or removing it moves the entity. */
        Table,
        /** A column with an enabled flag per entity. Turning it on or off does not move the entity. */
        Enableable,
        /**
         * Beside the archetypes, in its own set. Adding or removing it never moves the entity,
         * and its address is stable. For components that come and go often on few entities.
         */
        Sparse,
    };

    /**
     * The declaration on a component type. Put it on the struct as
     * `static constexpr Fling::ComponentInfo Component { .Name = "Mover" };`.
     */
    struct ComponentInfo
    {
        /**
         * The saved name. Level files use it. It must end with the C++ type name, after an
         * optional prefix and a dot: "MyGame.Health" or "Health" for struct Health (for a
         * template, the last part starts with it: "MyGame.Part3"). The compiler checks this. To rename, change it and
         * add the old name with TypeBuilder::FormerNames, so old levels still load.
         */
        const char* Name = nullptr;
        ComponentKind Kind = ComponentKind::Table;
        ComponentDomain Domain = ComponentDomain::Simulation;
        /** True if the type is never saved to a level (proxies, caches, queues). */
        bool Transient = false;
        /** Table components only: how the storage keeps it. */
        ComponentStorage Storage = ComponentStorage::Table;
        /**
         * Singletons and queues only. By default the engine makes the singleton (with its
         * default values) for the system that writes it, or the queue for the system that
         * consumes it, and removes it when the world shuts down. Set this to true when
         * code outside the systems makes and removes it (the physics module makes the
         * PhysicsWorld), so the engine leaves it alone.
         */
        bool External = false;
    };

    namespace Detail
    {
        template <typename T, typename = void>
        struct DefaultComponentTraits
        {
        };

        template <typename T, typename = void>
        struct DeclaredRequires
        {
            using Type = TypeList<>;
        };

        template <typename T>
        struct DeclaredRequires<T, std::void_t<typename T::Requires>>
        {
            using Type = typename T::Requires;
        };

        template <typename T>
        struct DefaultComponentTraits<T, std::void_t<decltype(T::Component)>>
        {
            static constexpr ComponentInfo Info = T::Component;
            /** T::Requires, or an empty list. See RequiredOf. */
            using Requires = typename DeclaredRequires<T>::Type;
        };
    } // namespace Detail

    /**
     * Where the engine finds the ComponentInfo for T. By default it is T::Component.
     * Specialize this for a type that you cannot edit, and give it a
     * `static constexpr ComponentInfo Info` (and, if it requires other components, a
     * `using Requires = TypeList<...>;`).
     */
    template <typename T>
    struct ComponentTraits : Detail::DefaultComponentTraits<T>
    {
    };

    /** True for a type that declares a ComponentInfo, either on the struct or through ComponentTraits. */
    template <typename T>
    concept Component = std::is_class_v<T> && !std::is_const_v<T> && !std::is_volatile_v<T> &&
                        std::is_same_v<std::remove_cv_t<decltype(ComponentTraits<T>::Info)>, ComponentInfo> &&
                        std::is_nothrow_move_constructible_v<T>;

    /**
     * True if a saved name ends with the C++ type name: the whole name, or a prefix, a
     * dot, and the type name. "Game.Health" and "Health" match "Health"; "Game.Hp" does not.
     * Each instance of a template needs its own name, so for a template (isTemplate) the
     * last part only starts with the type name: "Game.Part3" matches Part<3>.
     */
    constexpr bool SavedNameMatchesType(const StringView savedName, const StringView typeName, const bool isTemplate)
    {
        const size_t dot = savedName.rfind('.');
        const StringView last = dot == StringView::npos ? savedName : savedName.substr(dot + 1);
        if (dot == 0)
        {
            return false;
        }
        return isTemplate ? last.starts_with(typeName) : last == typeName;
    }

    namespace Detail
    {
        template <typename T>
        constexpr ComponentInfo CheckedComponentInfo()
        {
            constexpr ComponentInfo info = ComponentTraits<T>::Info;
            static_assert(info.Name == nullptr || SavedNameMatchesType(info.Name,
                                                                       ShortTypeNameOf<T>(),
                                                                       TypeNameOf<T>().find('<') != StringView::npos),
                          "A component's saved .Name must end with its C++ type name: \"Health\" or "
                          "\"MyGame.Health\" for struct Health. If you renamed the struct, change .Name too, and "
                          "add the old name with TypeBuilder::FormerNames so that old levels still load.");
            return info;
        }
    } // namespace Detail

    /** The ComponentInfo of T. The compiler checks that its saved name matches the type name. */
    template <Component T>
    inline constexpr ComponentInfo ComponentInfoOf = Detail::CheckedComponentInfo<T>();

    /** True if the declaration of T is complete and agrees with the shape of the struct. */
    template <Component T>
    inline constexpr bool IsWellFormedComponent =
        ComponentInfoOf<T>.Name != nullptr && !StringView(ComponentInfoOf<T>.Name).empty() &&
        (ComponentInfoOf<T>.Kind == ComponentKind::Tag) == std::is_empty_v<T>;

    /** The stable id of T, made from its saved name at compile time. */
    template <Component T>
    inline constexpr TypeHash TypeHashOf = HashName(ComponentInfoOf<T>.Name);

    /** True if T is a singleton or a queue (one per world). */
    template <Component T>
    inline constexpr bool IsSingletonComponent =
        ComponentInfoOf<T>.Kind == ComponentKind::Singleton || ComponentInfoOf<T>.Kind == ComponentKind::Queue;

    namespace Detail
    {
        template <typename T, typename = void>
        struct RequiredTypes
        {
            using Type = TypeList<>;
        };

        template <typename T>
        struct RequiredTypes<T, std::void_t<typename ComponentTraits<T>::Requires>>
        {
            using Type = typename ComponentTraits<T>::Requires;
        };
    } // namespace Detail

    /**
     * The components that an entity must have to have a T. Declare them on the struct:
     * `using Requires = Fling::TypeList<Fling::Transform>;` (a point light needs a place).
     * A component with no such line requires nothing.
     *
     * What the engine does with it:
     * - `storage.Create(...)` and `ctx.Spawn(...)` do not compile without the required components.
     * - To add T to an entity that does not have them, or to remove one of them from an
     *   entity that has T, is an error: a debug build asserts, and every build logs it.
     *   So add the required components first, and remove them last.
     * - The editor adds them with T, and does not remove one while T is there.
     * - A level that has T without them gets default ones, with a warning.
     */
    template <typename T>
    using RequiredOf = typename Detail::RequiredTypes<T>::Type;

    namespace Detail
    {
        /** True if T can be in a Requires list, or have one: a component on entities that is in the archetype. */
        template <typename T>
        consteval bool CanHaveRequirements()
        {
            if constexpr (Component<T>)
            {
                const ComponentInfo info = ComponentInfoOf<T>;
                return (info.Kind == ComponentKind::Table || info.Kind == ComponentKind::Tag) &&
                       info.Storage != ComponentStorage::Sparse;
            }
            return false;
        }

        template <typename T, typename... Rs>
        consteval bool CheckRequiresList(TypeList<Rs...>)
        {
            static_assert((Component<Rs> && ...), "A type in a component's Requires list is not a Fling::Component.");
            static_assert((!std::is_same_v<T, Rs> && ...), "A component cannot require itself.");
            static_assert(sizeof...(Rs) == 0 || CanHaveRequirements<T>(),
                          "Only a Table or Tag component (and not a Sparse one) can have a Requires list.");
            static_assert((CanHaveRequirements<Rs>() && ...),
                          "A component can require only Table and Tag components (and not Sparse ones).");
            static_assert((!Contains<RequiredOf<Rs>, T> && ...),
                          "Two components require each other. Remove one of the two requirements, or make them "
                          "one component.");
            return true;
        }

        /** The rules for T's Requires list, as static_asserts. True so that it can be in a static_assert. */
        template <typename T>
        consteval bool CheckRequires()
        {
            return CheckRequiresList<T>(RequiredOf<T>{});
        }

        template <typename List, typename... Present>
        inline constexpr bool AllIn = false;

        template <typename... Rs, typename... Present>
        inline constexpr bool AllIn<TypeList<Rs...>, Present...> = (Contains<TypeList<Present...>, Rs> && ...);
    } // namespace Detail

    /** True if every component that T requires is one of Present. */
    template <typename T, typename... Present>
    inline constexpr bool HasRequired = Detail::AllIn<RequiredOf<T>, Present...>;
} // namespace Fling
