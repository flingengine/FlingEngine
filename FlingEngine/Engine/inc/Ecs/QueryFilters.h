#pragma once

#include "Ecs/Component.h"
#include "Ecs/TypeList.h"

#include <type_traits>

namespace Fling
{
    /** A query filter: only entities that also have each T. The components are not returned. */
    template <typename... Ts>
    struct With
    {
    };

    /** A query filter: only entities that have none of the Ts. */
    template <typename... Ts>
    struct Without
    {
    };

    namespace Detail
    {
        /** True for a component that is on entities and has data: what a query returns. A const T counts. */
        template <typename T>
        inline constexpr bool IsTableComponent =
            Component<std::remove_const_t<T>> && ComponentInfoOf<std::remove_const_t<T>>.Kind == ComponentKind::Table;

        /** True for a component that is on entities, with data or a tag: what With and Without take. */
        template <typename T>
        inline constexpr bool IsFilterComponent = Component<T> && (ComponentInfoOf<T>.Kind == ComponentKind::Table ||
                                                                   ComponentInfoOf<T>.Kind == ComponentKind::Tag);

        template <typename F>
        struct FilterTypes
        {
            static constexpr bool IsFilter = false;
        };

        template <typename... Ts>
        struct FilterTypes<With<Ts...>>
        {
            static constexpr bool IsFilter = true;
            using WithList = TypeList<Ts...>;
            using WithoutList = TypeList<>;
        };

        template <typename... Ts>
        struct FilterTypes<Without<Ts...>>
        {
            static constexpr bool IsFilter = true;
            using WithList = TypeList<>;
            using WithoutList = TypeList<Ts...>;
        };

        template <typename... Lists>
        struct Concat
        {
            using Type = TypeList<>;
        };

        template <typename... As>
        struct Concat<TypeList<As...>>
        {
            using Type = TypeList<As...>;
        };

        template <typename... As, typename... Bs, typename... Rest>
        struct Concat<TypeList<As...>, TypeList<Bs...>, Rest...>
        {
            using Type = typename Concat<TypeList<As..., Bs...>, Rest...>::Type;
        };

        /** True if every F is a With<...> or a Without<...>. */
        template <typename... Filters>
        inline constexpr bool AreFilters = (FilterTypes<Filters>::IsFilter && ...);

        /** Every type that the With<...> filters in Filters name, as one TypeList. */
        template <typename... Filters>
        using WithTypes = typename Concat<typename FilterTypes<Filters>::WithList...>::Type;

        /** Every type that the Without<...> filters in Filters name, as one TypeList. */
        template <typename... Filters>
        using WithoutTypes = typename Concat<typename FilterTypes<Filters>::WithoutList...>::Type;
    } // namespace Detail
} // namespace Fling
