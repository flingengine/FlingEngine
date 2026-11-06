#pragma once

#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include <immintrin.h>
#elif defined(_M_ARM64)
#include <intrin.h>
#endif

namespace Fling
{
    /**
     * Tell the CPU that this is a spin loop, which eases the spin on the other
     * hyperthread of the core. Do not give the core to the scheduler here
     * (std::this_thread::yield, SwitchToThread). With every core busy, that
     * can delay the return by a full scheduling quantum, which is milliseconds.
     *
     * This header does not include Platform.h, so that the threading headers
     * do not pull Windows.h into every file that includes them.
     */
    inline void CpuRelax()
    {
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        _mm_pause();
#elif defined(_M_ARM64)
        __yield();
#elif defined(__aarch64__) || defined(__arm__)
        asm volatile("yield");
#endif
    }
} // namespace Fling
