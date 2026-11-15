#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Input/InputSlice.h"

#include <memory>

namespace Fling
{
    class InputMessageHandler;

    /**
     * Reads the input after a cut in the message queue, up to now, without consuming it:
     * the input that the simulation has not consumed yet. It has no cursor: each Read
     * starts at the sequence it is given. So it never hides a message from another reader
     * (the fixed tick's TickInputBuilder), and the caller decides where the cut is. The
     * frame side passes the cut of the tick whose state it shows (the TickStamp it shows),
     * never a "latest cut" of another thread: the cut and the state must come from the
     * same tick. A frame-rate reader makes the slice again from scratch each frame, so it
     * holds no state of its own and nothing in it is counted twice when the simulation
     * catches up.
     *
     * Safe on any thread, also while the writer pushes: it reads only messages that the
     * writer has completely written.
     */
    class CORE_API PendingInputReader
    {
    public:
        explicit PendingInputReader(std::shared_ptr<const InputMessageHandler> t_Handler);

        /**
         * Fill t_Out with every message after t_AfterSequence, up to the newest one, with
         * InputSlice::Append: the same filter as the tick, so the two sides drop exactly
         * the same messages. If the queue overwrote messages after t_AfterSequence, t_Out
         * is empty, with Overran set, up to the newest message (InputSlice::Overran).
         *
         * @param t_AfterSequence  The cut: Cut.LastSequence of the tick whose state the caller shows
         * @param t_Forced         Treat these devices as captured too, whatever the stream says (tests and tools)
         */
        void Read(uint64 t_AfterSequence, InputSlice& t_Out, InputCapture t_Forced = InputCapture::None) const;

    private:
        std::weak_ptr<const InputMessageHandler> m_Handler;
    };
} // namespace Fling
