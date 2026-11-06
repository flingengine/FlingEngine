#pragma once

#include <cstdint>
#include <string_view>

namespace Fling
{
    /** A stable 64-bit id for a component type. It is made from the saved name, so it is the same on every platform and
     * build. */
    using TypeHash = std::uint64_t;

    /** 64-bit FNV-1a hash of a name. It runs at compile time. */
    constexpr TypeHash HashName(std::string_view name)
    {
        TypeHash hash = 0xcbf29ce484222325ull;
        for (const char c : name)
        {
            hash ^= static_cast<std::uint8_t>(c);
            hash *= 0x100000001b3ull;
        }
        return hash;
    }
} // namespace Fling
