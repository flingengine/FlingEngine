#pragma once

#include "Ecs/TypeList.h"

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
    } // namespace Detail
} // namespace Fling
