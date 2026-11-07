#pragma once

#include "Ecs/TypeHash.h"
#include "FlingTypes.h"
#include "Strings/String.h"

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
        /** The saved name. Level files use it, so do not change it. */
        const char* Name = nullptr;
        ComponentKind Kind = ComponentKind::Table;
        ComponentDomain Domain = ComponentDomain::Simulation;
        /** True if the type is never saved to a level (proxies, caches, queues). */
        bool Transient = false;
        /** Table components only: how the storage keeps it. */
        ComponentStorage Storage = ComponentStorage::Table;
    };

    namespace Detail
    {
        template <typename T, typename = void>
        struct DefaultComponentTraits
        {
        };

        template <typename T>
        struct DefaultComponentTraits<T, std::void_t<decltype(T::Component)>>
        {
            static constexpr ComponentInfo Info = T::Component;
        };
    } // namespace Detail

    /**
     * Where the engine finds the ComponentInfo for T. By default it is T::Component.
     * Specialize this for a type that you cannot edit, and give it a
     * `static constexpr ComponentInfo Info`.
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

    /** The ComponentInfo of T. */
    template <Component T>
    inline constexpr ComponentInfo ComponentInfoOf = ComponentTraits<T>::Info;

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
} // namespace Fling
