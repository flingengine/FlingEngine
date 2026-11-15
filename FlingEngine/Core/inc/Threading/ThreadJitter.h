#pragma once

#include "FlingTypes.h"
#include "Threading/CpuRelax.h"

#include <chrono>
#include <random>
#include <thread>

namespace Fling
{
    /**
     * Random short sleeps at chosen points of a thread's loop, to shake out races and to
     * prove that results do not depend on thread timing (--thread-jitter). It has its own
     * generator: it never draws from the simulation's or the global random stream. Each
     * thread owns one; it is not thread safe.
     */
    class ThreadJitter
    {
    public:
        ThreadJitter() = default;

        ThreadJitter(const uint32 t_Seed, const uint32 t_MaxMicroseconds)
            : m_Random(t_Seed), m_MaxMicroseconds(t_MaxMicroseconds)
        {
        }

        bool IsOn() const { return m_MaxMicroseconds > 0; }

        /**
         * Wait 0 to the maximum microseconds. Nothing when off. A wait under a millisecond
         * spins, because an OS sleep can last a whole timer period (15.6 ms on Windows) and
         * would hide the short waits.
         */
        void Sleep()
        {
            if (!IsOn())
            {
                return;
            }
            const std::chrono::microseconds Wait(m_Random() % (m_MaxMicroseconds + 1));
            if (Wait >= std::chrono::milliseconds(1))
            {
                std::this_thread::sleep_for(Wait);
                return;
            }
            const auto End = std::chrono::steady_clock::now() + Wait;
            while (std::chrono::steady_clock::now() < End)
            {
                CpuRelax();
            }
        }

    private:
        std::mt19937 m_Random;
        uint32 m_MaxMicroseconds = 0;
    };
} // namespace Fling
