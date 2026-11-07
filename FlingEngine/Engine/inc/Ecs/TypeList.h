#pragma once

#include <cstddef>
#include <type_traits>

namespace Fling
{
    /** A compile-time list of types. It holds no data. */
    template <typename... Ts>
    struct TypeList
    {
        static constexpr std::size_t Size = sizeof...(Ts);
    };

    /** True if T is one of the types in List. */
    template <typename List, typename T>
    inline constexpr bool Contains = false;

    template <typename... Ts, typename T>
    inline constexpr bool Contains<TypeList<Ts...>, T> = (std::is_same_v<Ts, T> || ...);

    /** True if no type is in Ts more than once. */
    template <typename... Ts>
    inline constexpr bool AllUnique = true;

    template <typename T, typename... Rest>
    inline constexpr bool AllUnique<T, Rest...> = (!std::is_same_v<T, Rest> && ...) && AllUnique<Rest...>;
} // namespace Fling
