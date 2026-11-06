#pragma once

#include <cstdint>
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
        std::uint32_t Index = UINT32_MAX;
        std::uint32_t Generation = UINT32_MAX;

        static const Entity Null;

        constexpr bool IsNull() const { return Index == UINT32_MAX; }

        constexpr std::uint64_t ToBits() const { return (static_cast<std::uint64_t>(Generation) << 32) | Index; }

        static constexpr Entity FromBits(const std::uint64_t bits)
        {
            return Entity{static_cast<std::uint32_t>(bits & 0xffffffffull), static_cast<std::uint32_t>(bits >> 32)};
        }

        friend constexpr bool operator==(Entity, Entity) = default;
    };

    inline constexpr Entity Entity::Null{};

    static_assert(sizeof(Entity) == 8 && std::is_trivially_copyable_v<Entity>);
} // namespace Fling
