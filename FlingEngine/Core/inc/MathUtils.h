#pragma once

#include <cstddef>
#include <type_traits>

/**
 * Fling's scalar math: min, max, clamp, and the power-of-two and alignment math that
 * allocators and storage use. Use these, not std::min, std::max, or hand-written
 * versions. Vectors, matrices, and quaternions are the glm types in FlingMath.h,
 * which includes this file. It is small, so headers that need only scalar math
 * include it alone.
 */
namespace Fling::Math
{
    template <typename T>
    constexpr T Min(const T a, const T b)
    {
        return b < a ? b : a;
    }

    template <typename T>
    constexpr T Max(const T a, const T b)
    {
        return a < b ? b : a;
    }

    template <typename T>
    constexpr T Min(const T a, const T b, const T c)
    {
        return Min(Min(a, b), c);
    }

    template <typename T>
    constexpr T Max(const T a, const T b, const T c)
    {
        return Max(Max(a, b), c);
    }

    /** Value limited to [Low, High]. Low must not be more than High. */
    template <typename T>
    constexpr T Clamp(const T value, const T low, const T high)
    {
        return value < low ? low : (high < value ? high : value);
    }

    template <typename T>
    constexpr bool IsPowerOfTwo(const T value)
    {
        static_assert(std::is_unsigned_v<T>, "IsPowerOfTwo takes an unsigned integer");
        return value != 0 && (value & (value - 1)) == 0;
    }

    /** The smallest power of two that is at least value (1 for 0). */
    template <typename T>
    constexpr T NextPowerOfTwo(const T value)
    {
        static_assert(std::is_unsigned_v<T>, "NextPowerOfTwo takes an unsigned integer");
        T result = 1;
        while (result < value)
        {
            result <<= 1;
        }
        return result;
    }

    /** Value rounded up to a multiple of alignment, which is a power of two. */
    template <typename T>
    constexpr T AlignUp(const T value, const T alignment)
    {
        static_assert(std::is_unsigned_v<T>, "AlignUp takes an unsigned integer");
        return (value + alignment - 1) & ~(alignment - 1);
    }

    /** Value rounded down to a multiple of alignment, which is a power of two. */
    template <typename T>
    constexpr T AlignDown(const T value, const T alignment)
    {
        static_assert(std::is_unsigned_v<T>, "AlignDown takes an unsigned integer");
        return value & ~(alignment - 1);
    }

    /** Numerator / Denominator, rounded up. For any positive Denominator, not only powers of two. */
    template <typename T>
    constexpr T DivideRoundUp(const T numerator, const T denominator)
    {
        static_assert(std::is_unsigned_v<T>, "DivideRoundUp takes an unsigned integer");
        return (numerator + denominator - 1) / denominator;
    }
} // namespace Fling::Math
