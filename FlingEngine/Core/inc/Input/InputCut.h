#pragma once

#include "FlingTypes.h"

namespace Fling
{
    /**
     * A range of the input message queue: the messages with a sequence in
     * (AfterSequence, LastSequence]. A fixed tick's cut is the range it consumed; the
     * pending input of a frame is the range after the cut of the tick it shows. When the
     * range holds no message, the two are equal.
     */
    struct InputCut
    {
        /** The last message before the range: the LastSequence of the range before it. */
        uint64 AfterSequence = 0;

        /** The last message in the range. */
        uint64 LastSequence = 0;

        bool operator==(const InputCut&) const = default;
    };
} // namespace Fling
