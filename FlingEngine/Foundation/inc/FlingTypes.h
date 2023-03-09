#pragma once

#include <cstdint>
#include <limits>
#include <stdexcept>
#include <inttypes.h>
#include <entt/core/hashed_string.hpp>
#include <entt/entity/helper.hpp>

// Integer typedefs for ease of use
typedef uint8_t                 uint8;
typedef unsigned short          uint16;
typedef uint32_t                uint32;
typedef uint64_t                uint64;

typedef int8_t                  int8;
typedef signed short            int16;
typedef int32_t                 int32;
typedef int64_t                 int64;

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
 * @return An @ref uint32 representation of the same value
 */
template <class T>
uint32 to_u32(T value)
{
	static_assert(std::is_arithmetic<T>::value, "T must be numeric");

	if (static_cast<uintmax_t>(value) > static_cast<uintmax_t>(std::numeric_limits<uint32>::max()))
	{
		throw std::runtime_error("to_u32() failed, value is too big to be converted to uint32");
	}

	return static_cast<uint32>(value);
}

// PRIVATE_WITH_EDITOR gives editor access to any properties while keeping encapsulation 
// during gameplay and runtime games.
#if WITH_EDITOR

    #define PRIVATE_WITH_EDITOR friend class BaseEditor; private
                                
#else

    #define PRIVATE_WITH_EDITOR private

#endif


#if WITH_EDITOR

#define PROTECTED_WITH_EDITOR friend class BaseEditor; protected

#else

#define PROTECTED_WITH_EDITOR protected

#endif