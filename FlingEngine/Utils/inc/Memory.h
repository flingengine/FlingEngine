#pragma once

#include <stdlib.h>

namespace Fling
{
    void* AlignedAlloc(size_t t_Size, size_t t_Alignment);

    void AlignedFree(void* t_Data);

#if FLING_LINUX
#define leading_zeroes(x) ((x) == 0 ? 32 : __builtin_clz(x))
#define trailing_zeroes(x) ((x) == 0 ? 32 : __builtin_ctz(x))
#define trailing_ones(x) __builtin_ctz(~(x))
#elif FLING_WINDOWS
#include <intrin.h>

    namespace Internal
    {
        static inline uint32_t clz(uint32_t x)
        {
            unsigned long result;
            if (_BitScanReverse(&result, x))
                return 31 - result;
            else
                return 32;
        }

        static inline uint32_t ctz(uint32_t x)
        {
            unsigned long result;
            if (_BitScanForward(&result, x))
                return result;
            else
                return 32;
        }
    }

#define leading_zeroes(x) Fling::Internal::clz(x)
#define trailing_zeroes(x) Fling::Internal::ctz(x)
#define trailing_ones(x) Fling::Internal::ctz(~(x))

#endif

    template<typename T>
    inline void for_each_bit(uint32_t value, const T& func)
    {
        while (value)
        {
            UINT32 bit = trailing_zeroes(value);
            func(bit);
            value &= ~(1u << bit);
        }
    }

}   // namespace Fling