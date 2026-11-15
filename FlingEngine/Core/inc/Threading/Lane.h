#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"

/**
 * FLING_LANE_CHECKS is 1 in builds with asserts (no NDEBUG) that are not shipping
 * builds. Then FLING_LANE_CHECK(owner) checks the lane of the calling thread. In
 * other builds the macro is empty and does not evaluate its argument, and the
 * storage has no owner field. The tag itself (LaneCheck::Scope, LaneCheck::Current)
 * is in every build: TickInputProvider reads it to know if it runs on the tick lane.
 */
#if !defined(NDEBUG) && !defined(FLING_SHIPPING)
#define FLING_LANE_CHECKS 1
#else
#define FLING_LANE_CHECKS 0
#endif

namespace Fling
{
    /**
     * Which part of the frame loop a thread runs (Docs/threading-model.md section 7).
     * Game: the thread that runs the fixed tick (the game thread in Threaded play).
     * Frame: the main thread, which runs the frame systems and the renderer. A thread has no lane until a
     * LaneCheck::Scope gives it one.
     */
    enum class Lane : uint8
    {
        None,
        Game,
        Frame,
    };

    /**
     * A debug tag for each thread: the lane it runs. Data that belongs to one lane
     * (an EntityStorage) has an owner lane, and its entry points call
     * FLING_LANE_CHECK(owner). A thread tagged with another lane is a violation.
     * A thread with no tag, and data with no owner, pass.
     */
    class CORE_API LaneCheck
    {
    public:
        /** The lane of this thread. None if no Scope is alive. */
        static Lane Current();

        /** True if this thread may touch data that owner owns. */
        static bool Allows(const Lane owner)
        {
            const Lane current = Current();
            return owner == Lane::None || current == Lane::None || current == owner;
        }

        /** Called on a violation. The default logs an error and asserts. */
        using ViolationHandler = void (*)(Lane owner, Lane current);

        /** Replace the handler (tests count violations). Null restores the default. Returns the old one. */
        static ViolationHandler SetViolationHandler(ViolationHandler handler);

        /** Calls the handler if this thread may not touch data that owner owns. */
        static void Check(const Lane owner)
        {
            if (!Allows(owner)) [[unlikely]]
            {
                ReportViolation(owner);
            }
        }

        static const char* NameOf(Lane lane);

        /** Gives this thread a lane until the scope ends. Scopes nest: the end restores the lane before. */
        class CORE_API Scope
        {
        public:
            explicit Scope(Lane lane);
            ~Scope();

            Scope(const Scope&) = delete;
            Scope& operator=(const Scope&) = delete;

        private:
            Lane m_Previous = Lane::None;
        };

    private:
        static void ReportViolation(Lane owner);
    };
} // namespace Fling

#if FLING_LANE_CHECKS
#define FLING_LANE_CHECK(owner) ::Fling::LaneCheck::Check(owner)
#else
#define FLING_LANE_CHECK(owner) ((void)0)
#endif
