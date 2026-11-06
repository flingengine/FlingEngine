#pragma once

#include "FlingTypes.h"

namespace Fling
{
    /**
     * The work that a Thread does.
     *
     * The thread calls Init, then Run when Init returns true, then Exit. Stop is
     * called from a different thread. It must make Run return soon.
     */
    class Runnable
    {
    public:
        virtual ~Runnable() = default;

        /**
         * Prepare the work on the new thread. Thread::Create waits for this call.
         * Return false to stop the thread before Run.
         */
        virtual bool Init() { return true; }

        /** Do the work. The return value is the exit code of the thread. */
        virtual uint32 Run() = 0;

        /** Ask Run to return. Another thread calls this, so it must be thread-safe. */
        virtual void Stop() {}

        /** Clean up on the thread after Run returns. */
        virtual void Exit() {}
    };
} // namespace Fling
