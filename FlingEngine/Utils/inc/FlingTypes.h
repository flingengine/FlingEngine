#pragma once

#include <cstdint>
#include <inttypes.h>
#include <entt/core/hashed_string.hpp>

// Integer typedefs for ease of use
typedef uint8_t                 UINT8;
typedef unsigned short          UINT16;
typedef uint32_t                UINT32;
typedef uint64_t                UINT64;

typedef int8_t                  INT8;
typedef signed short            INT16;
typedef int32_t                 INT32;
typedef int64_t                 INT64;

namespace Fling
{
    // Use hashed strings as Guid's
    typedef entt::hashed_string                     Guid;
    typedef entt::hashed_string::hash_type          Guid_Handle;
    static Guid INVALID_GUID = { "INVALID_GUID"_hs };
}