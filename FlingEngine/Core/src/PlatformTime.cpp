#include "Misc/SimulationCheck.h"
#include "PlatformTime.h"

#include <cassert>
#include <chrono>

namespace Fling::PlatformTime
{
    namespace
    {
        std::atomic<const VirtualClock*> s_Virtual{nullptr};
    } // namespace

    void VirtualClock::Set(const int64 t_NowNs)
    {
        assert(t_NowNs >= Now() && "A virtual clock must not go back");
        m_NowNs.store(t_NowNs, std::memory_order_relaxed);
    }

    int64 SteadyNanoseconds()
    {
        using namespace std::chrono;
        return static_cast<int64>(duration_cast<nanoseconds>(steady_clock::now().time_since_epoch()).count());
    }

    int64 NowNanoseconds()
    {
        FLING_SIMULATION_FORBID("Simulation code must not read the platform clock. Use the SimTime singleton.");
        const VirtualClock* Clock = s_Virtual.load(std::memory_order_acquire);
        return Clock != nullptr ? Clock->Now() : SteadyNanoseconds();
    }

    void UseVirtualClock(const VirtualClock* t_Clock)
    {
        s_Virtual.store(t_Clock, std::memory_order_release);
    }

    const VirtualClock* GetVirtualClock()
    {
        return s_Virtual.load(std::memory_order_acquire);
    }

    ScopedVirtualClock::ScopedVirtualClock(const int64 t_StartNs) : m_Clock(t_StartNs), m_Previous(GetVirtualClock())
    {
        UseVirtualClock(&m_Clock);
    }

    ScopedVirtualClock::~ScopedVirtualClock()
    {
        UseVirtualClock(m_Previous);
    }
} // namespace Fling::PlatformTime
