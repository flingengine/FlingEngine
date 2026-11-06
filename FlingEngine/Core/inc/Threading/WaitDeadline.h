#pragma once

#include <chrono>

namespace Fling::Detail
{
    /**
     * Get the steady_clock time at which a wait of t_Timeout ends.
     *
     * A timeout that does not fit gives time_point::max(), so a very long
     * timeout (for example hours::max()) waits forever and does not overflow
     * to a time in the past. A timeout of 0 or less gives the current time.
     */
    template <typename T_Rep, typename T_Period>
    std::chrono::steady_clock::time_point DeadlineAfter(const std::chrono::duration<T_Rep, T_Period>& t_Timeout)
    {
        using Clock = std::chrono::steady_clock;
        const Clock::time_point Now = Clock::now();
        if (t_Timeout <= std::chrono::duration<T_Rep, T_Period>::zero())
        {
            return Now;
        }

        // Compare as double, because a conversion of t_Timeout to Clock::duration
        // can overflow. The margin of one hour absorbs the rounding of double.
        const Clock::duration Headroom = Clock::time_point::max() - Now;
        const std::chrono::duration<double> Limit = Headroom - std::chrono::hours(1);
        if (std::chrono::duration<double>(t_Timeout) >= Limit)
        {
            return Clock::time_point::max();
        }
        return Now + std::chrono::ceil<Clock::duration>(t_Timeout);
    }
} // namespace Fling::Detail
