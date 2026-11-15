#pragma once

#include "CoreAPI.h"
#include "FlingMath.h"
#include "FlingTypes.h"
#include "Strings/String.h"

namespace Fling
{
    /**
     * A random number stream for simulation code. Draw n of a stream is a pure function
     * of (seed, stream, n): a counter mixed by a 64-bit hash, with no hidden state and no
     * std distribution. So it gives the same numbers on every platform, compiler and
     * thread, two streams never affect each other, and its whole state is two numbers
     * (copy it, save it with a snapshot, set it back for a rollback).
     *
     * Seed it from the run seed (Random::GetSeed, set by --seed) and give each user its
     * own stream (StreamOf("Sandbox.LightingTest")), so adding draws in one place does
     * not change another. The global Random is for cosmetic and editor code only.
     */
    class CORE_API SimRandom
    {
    public:
        SimRandom(uint64 t_Seed, uint64 t_Stream);

        /** A stream id from a name (FNV-1a), so code can name its stream instead of picking a number. */
        static constexpr uint64 StreamOf(const StringView t_Name)
        {
            uint64 Hash = 14695981039346656037ull;
            for (const char Character : t_Name)
            {
                Hash = (Hash ^ static_cast<uint8>(Character)) * 1099511628211ull;
            }
            return Hash;
        }

        /** The next 64 random bits. */
        uint64 NextU64();

        /** A float in [0, 1): 24 random bits. */
        float NextFloat01();

        /** A float in [t_Min, t_Max). The bounds can be in either order. */
        float Range(float t_Min, float t_Max);

        /** An int in [t_Min, t_Max], both included. The bounds can be in either order. */
        int32 RangeInclusive(int32 t_Min, int32 t_Max);

        /** Each component in [t_Min, t_Max) of its own, drawn x, then y, then z. */
        glm::vec3 Range(const glm::vec3& t_Min, const glm::vec3& t_Max);

        /** The number of draws so far. Set it back with SetCounter to repeat them. */
        uint64 GetCounter() const { return m_Counter; }

        void SetCounter(uint64 t_Counter) { m_Counter = t_Counter; }

    private:
        uint64 m_Key = 0;
        uint64 m_Counter = 0;
    };
} // namespace Fling
