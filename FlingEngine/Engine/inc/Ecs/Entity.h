#pragma once

#include "FlingTypes.h"

#include <compare>
#include <functional>
#include <limits>
#include <type_traits>

namespace Fling
{
    /**
     * A handle to an entity in one world. It holds no data and has no way to get
     * components: use the system context for that.
     *
     * Index is a slot in the world's location table, so a lookup is one array
     * read. Generation goes up each time the slot is used again, so an old handle
     * to a destroyed entity never finds the entity that uses the slot now.
     */
    struct Entity
    {
        uint32 Index = std::numeric_limits<uint32>::max();
        uint32 Generation = std::numeric_limits<uint32>::max();

        static const Entity Null;

        constexpr bool IsNull() const { return Index == std::numeric_limits<uint32>::max(); }

        constexpr uint64 ToBits() const { return (static_cast<uint64>(Generation) << 32) | Index; }

        static constexpr Entity FromBits(const uint64 bits)
        {
            return Entity{static_cast<uint32>(bits), static_cast<uint32>(bits >> 32)};
        }

        friend constexpr bool operator==(Entity, Entity) = default;

        /** Orders by index, then generation. Use it for sorting (for example, deterministic contact order). */
        friend constexpr auto operator<= > (Entity, Entity) = default;
    };

    inline constexpr Entity Entity::Null{};

    static_assert(sizeof(Entity) == 8 && std::is_trivially_copyable_v<Entity>);
} // namespace Fling

template <>
struct std::hash<Fling::Entity>
{
    size_t operator()(const Fling::Entity e) const noexcept { return std::hash<uint64>{}(e.ToBits()); }
};
