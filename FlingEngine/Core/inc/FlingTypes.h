#pragma once

#include "FlingIntegers.h"

#include <cstdint>
#include <entt/entity/helper.hpp>
#include <limits>
#include <stdexcept>

/**
 * Helper function to check size_t is correctly converted to uint32
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