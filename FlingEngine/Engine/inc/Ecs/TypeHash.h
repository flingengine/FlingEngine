#pragma once

#include "FlingTypes.h"
#include "Strings/String.h"

#include <string_view>

namespace Fling
{
    /** A stable 64-bit id for a component type. It is made from the saved name, so it is the same on every platform and
     * build. */
    using TypeHash = uint64;

    /** 64-bit FNV-1a hash of a name. It runs at compile time. */
    constexpr TypeHash HashName(StringView name)
    {
        TypeHash hash = TypeHash{0xcbf29ce484222325};
        for (const char c : name)
        {
            hash ^= static_cast<uint8>(c);
            hash *= TypeHash{0x100000001b3};
        }
        return hash;
    }
} // namespace Fling
