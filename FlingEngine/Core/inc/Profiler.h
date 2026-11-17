#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Strings/String.h"

#include <chrono>

#if WITH_PROFILER

namespace Fling
{
    /**
     * Instrumented timing for the engine. A FLING_PROFILE_SCOPE("Name") at the top of a
     * block times that block, on any thread. Scopes with the same name add up, so a scope in
     * a loop shows its total, its call count and its slowest call.
     *
     * It is off until --profile (or Profiler::SetEnabled) turns it on. An off scope costs one
     * relaxed atomic load. An on scope costs two clock reads and three stores into the
     * calling thread's own table: no lock, no allocation after the first call on a thread.
     * Shipping builds compile every scope out.
     *
     * Profiler::Report gives the table: Engine prints it at exit with --profile.
     */
    class Profiler
    {
    public:
        /** The most scopes (different names) that one run can have. */
        static constexpr uint32 MaxScopes = 256;

        static CORE_API bool IsEnabled();
        static CORE_API void SetEnabled(bool t_On);

        /** The id of a scope name. The name must stay valid for the whole run (a string literal). */
        static CORE_API uint32 Register(const char* t_Name);

        /** Add one timed call to the calling thread's table. */
        static CORE_API void Record(uint32 t_Id, int64 t_Nanoseconds);

        /** Forget every recorded call (the names stay). */
        static CORE_API void Reset();

        /**
         * A table of every scope, slowest total first: calls, total, average and max in
         * milliseconds, and each total as a share of t_WallSeconds when that is above 0.
         * Times add up across threads, so a total can pass the wall time.
         */
        static CORE_API String Report(double t_WallSeconds);
    };

    /** The timer behind FLING_PROFILE_SCOPE. */
    class ProfileScope
    {
    public:
        explicit ProfileScope(const uint32 t_Id) : m_Id(t_Id), m_On(Profiler::IsEnabled())
        {
            if (m_On)
            {
                m_Start = std::chrono::steady_clock::now();
            }
        }

        ~ProfileScope()
        {
            if (m_On)
            {
                const auto Elapsed = std::chrono::steady_clock::now() - m_Start;
                Profiler::Record(m_Id, std::chrono::duration_cast<std::chrono::nanoseconds>(Elapsed).count());
            }
        }

        ProfileScope(const ProfileScope&) = delete;
        ProfileScope& operator=(const ProfileScope&) = delete;

    private:
        uint32 m_Id;
        bool m_On;
        std::chrono::steady_clock::time_point m_Start;
    };
} // namespace Fling

#define FLING_PROFILE_CONCAT_INNER(A, B) A##B
#define FLING_PROFILE_CONCAT(A, B) FLING_PROFILE_CONCAT_INNER(A, B)
/** Time the rest of the enclosing block. Name is a string literal. */
#define FLING_PROFILE_SCOPE(Name)                                                                                      \
    static const uint32 FLING_PROFILE_CONCAT(FlingProfileId, __LINE__) = ::Fling::Profiler::Register(Name);            \
    ::Fling::ProfileScope FLING_PROFILE_CONCAT(FlingProfileScope,                                                      \
                                               __LINE__)(FLING_PROFILE_CONCAT(FlingProfileId, __LINE__))

#else

// Compiled out (WITH_PROFILER is 0): a scope is nothing, and there is no Profiler class
#define FLING_PROFILE_SCOPE(Name) ((void)0)

#endif
