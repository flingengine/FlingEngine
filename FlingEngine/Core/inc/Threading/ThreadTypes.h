#pragma once

#include "FlingTypes.h"

#include <chrono>
#include <string>

namespace Fling
{
    /** How closely a fixed-rate thread wakes at each tick. */
    enum class TickPrecision : uint8
    {
        /**
         * Sleep until the tick. A wake can be late by the OS timer resolution
         * (about 0.1 ms on Linux, about 0.5 ms on Windows). Uses almost no CPU.
         */
        Normal,

        /**
         * Sleep until a short margin before the tick, then spin until the tick.
         * A wake is late by only a few microseconds, but the thread uses a full
         * CPU core for the margin of each tick.
         */
        Precise,
    };

    /**
     * A fixed tick rate, for example TickRate::Hz(60). A Period of 0 means that
     * there is no fixed rate.
     */
    struct TickRate
    {
        std::chrono::nanoseconds Period{0};

        TickPrecision Precision = TickPrecision::Normal;

        /**
         * With TickPrecision::Precise: how long before each tick the sleep ends
         * and the spin starts. It must cover how late a sleep can wake. A larger
         * window costs more CPU on every tick. It is capped at half a Period, so
         * a high rate does not turn into a constant spin.
         */
        std::chrono::nanoseconds SpinWindow = std::chrono::milliseconds(1);

        /** The rate t_Hz ticks each second. t_Hz <= 0 gives no fixed rate. */
        static TickRate Hz(double t_Hz, TickPrecision t_Precision = TickPrecision::Normal)
        {
            if (t_Hz <= 0.0)
            {
                return {};
            }
            TickRate Rate;
            Rate.Period =
                std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::duration<double>(1.0 / t_Hz));
            Rate.Precision = t_Precision;
            return Rate;
        }

        bool IsFixed() const { return Period.count() > 0; }

        bool IsPrecise() const { return Precision == TickPrecision::Precise; }
    };

    /**
     * Engine ID of a thread. The engine gives the IDs in creation order, so they
     * are the same on every platform and in every run. The OS ID is different.
     * Use Thread::GetNativeId for the OS ID.
     */
    using ThreadId = uint32;

    /** No thread has this ID. CurrentId returns it on a thread that the engine did not start. */
    constexpr ThreadId InvalidThreadId = 0;

    /** The ID of the thread that called Thread::InitGameThread. */
    constexpr ThreadId GameThreadId = 1;

    /** Scheduling priority of a thread. Linux can only lower a priority without privileges. */
    enum class ThreadPriority : uint8
    {
        Lowest,
        BelowNormal,
        Normal,
        AboveNormal,
        Highest,
        TimeCritical,
    };

    /** The settings that Thread::Create uses to start a thread. */
    struct ThreadDesc
    {
        /** The full name is kept in the engine. The OS name can be shorter (15 bytes on Linux). */
        std::string Name;

        /** Stack size in bytes. 0 uses the platform default. */
        uint32 StackSize = 0;

        ThreadPriority Priority = ThreadPriority::Normal;

        /** One bit for each CPU that the thread can run on. 0 does not change the affinity. */
        uint64 AffinityMask = 0;
    };
} // namespace Fling
