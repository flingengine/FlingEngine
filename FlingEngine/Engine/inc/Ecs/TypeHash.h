#pragma once

#include "FlingTypes.h"
#include "Strings/String.h"

#include <string_view>

namespace Fling
{
    /** A stable 64-bit id for a component type. It is made from the saved name, so it is the same on every platform and
     * build. */
    using TypeHash = uint64;

    /** The start value of HashName (the FNV-1a offset basis): the hash of the empty name. */
    inline constexpr TypeHash HashNameSeed = 0xcbf29ce484222325;

    /** One FNV-1a step of HashName: the hash after one more character. */
    constexpr TypeHash HashNameStep(const TypeHash hash, const char c)
    {
        return (hash ^ static_cast<uint8>(c)) * TypeHash{0x100000001b3};
    }

    /** 64-bit FNV-1a hash of a name. It runs at compile time. */
    constexpr TypeHash HashName(StringView name)
    {
        TypeHash hash = HashNameSeed;
        for (const char c : name)
        {
            hash = HashNameStep(hash, c);
        }
        return hash;
    }
} // namespace Fling
