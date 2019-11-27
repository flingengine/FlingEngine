#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
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

#define HS(str) entt::hashed_string { (str) }

namespace Fling
{
    // Use hashed strings as Guid's
    typedef entt::hashed_string                     Guid;
    typedef entt::hashed_string::hash_type          Guid_Handle;
    static Guid INVALID_GUID = { "INVALID_GUID"_hs };
}

/**
 * @brief Helper function to check size_t is correctly converted to uint32_t
 * @param value Value of type @ref size_t to convert
 * @return An @ref uint32_t representation of the same value
 */
template <class T>
UINT32 to_u32(T value)
{
	static_assert(std::is_arithmetic<T>::value, "T must be numeric");

	if (static_cast<uintmax_t>(value) > static_cast<uintmax_t>(std::numeric_limits<UINT32>::max()))
	{
		throw std::runtime_error("to_u32() failed, value is too big to be converted to UINT32");
	}

	return static_cast<UINT32>(value);
}