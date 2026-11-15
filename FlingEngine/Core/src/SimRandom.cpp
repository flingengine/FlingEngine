#include "SimRandom.h"

namespace Fling
{
    namespace
    {
        /** The SplitMix64 finalizer: every input bit changes about half of the output bits. */
        constexpr uint64 Mix(uint64 t_Value)
        {
            t_Value = (t_Value ^ (t_Value >> 30)) * 0xBF58476D1CE4E5B9ull;
            t_Value = (t_Value ^ (t_Value >> 27)) * 0x94D049BB133111EBull;
            return t_Value ^ (t_Value >> 31);
        }

        constexpr uint64 Golden = 0x9E3779B97F4A7C15ull;
    } // namespace

    SimRandom::SimRandom(const uint64 t_Seed, const uint64 t_Stream) : m_Key(Mix(Mix(t_Seed) ^ (t_Stream * Golden)))
    {
    }

    uint64 SimRandom::NextU64()
    {
        return Mix(m_Key + (m_Counter++ + 1) * Golden);
    }

    float SimRandom::NextFloat01()
    {
        // 24 bits fill a float mantissa exactly, so the result is below 1
        return static_cast<float>(NextU64() >> 40) * (1.0f / 16777216.0f);
    }

    float SimRandom::Range(const float t_Min, const float t_Max)
    {
        const float Low = t_Min < t_Max ? t_Min : t_Max;
        const float High = t_Min < t_Max ? t_Max : t_Min;
        const float Value = Low + (High - Low) * NextFloat01();
        // Rounding can reach High for a wide range: keep the upper bound open
        return Value < High ? Value : Low;
    }

    int32 SimRandom::RangeInclusive(const int32 t_Min, const int32 t_Max)
    {
        const int64 Low = t_Min < t_Max ? t_Min : t_Max;
        const int64 High = t_Min < t_Max ? t_Max : t_Min;
        const uint64 Span = static_cast<uint64>(High - Low) + 1;
        // Multiply-shift of 32 random bits: the span is at most 2^32, so the product fits in 64 bits
        const uint64 Pick = ((NextU64() >> 32) * Span) >> 32;
        return static_cast<int32>(Low + static_cast<int64>(Pick));
    }

    glm::vec3 SimRandom::Range(const glm::vec3& t_Min, const glm::vec3& t_Max)
    {
        glm::vec3 Result;
        Result.x = Range(t_Min.x, t_Max.x);
        Result.y = Range(t_Min.y, t_Max.y);
        Result.z = Range(t_Min.z, t_Max.z);
        return Result;
    }
} // namespace Fling
