#pragma once

#include "FlingTypes.h"

namespace Fling
{
    /**
     * The state of one key on one device, as one consumer sees it. The counts are the
     * number of edges since that consumer last cleared them, so a press and a release
     * between two reads give PressCount 1, ReleaseCount 1, and IsHeld false.
     */
    struct InputKeyState
    {
        bool IsHeld = false;

        uint16 PressCount = 0;

        uint16 ReleaseCount = 0;

        /** Operating system key repeats. Not presses. */
        uint16 RepeatCount = 0;

        /** Axis value, or 1 while a button is held and 0 when it is not. */
        float Value = 0.0f;

        /** TimeNs of the last message that changed this key. */
        int64 LastChangeTimeNs = 0;

        bool WasPressed() const { return PressCount > 0; }

        bool WasReleased() const { return ReleaseCount > 0; }

        /** Reset the edge counts. Held state and Value stay. */
        void ClearEdges()
        {
            PressCount = 0;
            ReleaseCount = 0;
            RepeatCount = 0;
        }
    };
} // namespace Fling
