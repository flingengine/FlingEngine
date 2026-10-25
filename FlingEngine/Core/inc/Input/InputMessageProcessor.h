#pragma once

#include "FlingTypes.h"
#include "Input/InputDeviceStates.h"
#include "Input/InputMessage.h"
#include <limits>
#include <memory>

namespace Fling
{
    class InputMessageQueue;

    /**
     * One consumer of the input message queue: the game tick, the render thread, the
     * editor, a recorder. Each processor has its own cursor and its own accumulated
     * device state, so consumers read at their own rate and never take a message from
     * another consumer.
     *
     * Create one with InputMessageHandler::CreateProcessor. It keeps only a weak_ptr to
     * the queue. After the queue is destroyed it reads nothing, and IsValid is false.
     */
    class InputMessageProcessor
    {
    public:
        /**
         * @param t_Queue           The queue to read
         * @param t_StartAfter      The first message read is the one after this sequence
         */
        InputMessageProcessor(std::weak_ptr<const InputMessageQueue> t_Queue, uint64 t_StartAfter);

        /** False if the queue was destroyed (for example, Input was shut down). */
        bool IsValid() const { return !m_Queue.expired(); }

        /**
         * Copy messages after the cursor with TimeNs < t_UntilNs into t_Out, up to t_Max.
         * Does not move the cursor or change the state.
         *
         * @return The number of messages copied
         */
        uint32 Read(InputMessage* t_Out, uint32 t_Max, int64 t_UntilNs = std::numeric_limits<int64>::max()) const;

        /**
         * Move the cursor past every message up to and including t_Sequence, and apply
         * them to the state. If some of those messages were overwritten, it stops and
         * Overran() becomes true.
         */
        void Consume(uint64 t_Sequence);

        /**
         * Consume every message with TimeNs < t_UntilNs.
         *
         * @return The number of messages consumed
         */
        uint32 ConsumeUntil(int64 t_UntilNs = std::numeric_limits<int64>::max());

        /** Clear the edge counts and summed mouse values of the state. Call after each use of the state. */
        void ClearEdges() { m_State.ClearEdges(); }

        const InputDeviceStates& GetState() const { return m_State; }

        /** Sequence of the last consumed message. */
        uint64 GetCursor() const { return m_Cursor; }

        /** True if the queue overwrote messages that this processor did not consume. */
        bool Overran() const;

        /**
         * Recover after an overrun: apply transition messages from the current state to
         * t_Latest (so held keys are correct and edges show the change), then move the
         * cursor to t_LatestSequence. Short taps in the lost range are gone.
         */
        void Resync(const InputDeviceStates& t_Latest, uint64 t_LatestSequence);

    private:
        std::weak_ptr<const InputMessageQueue> m_Queue;

        uint64 m_Cursor = 0;

        bool m_Overran = false;

        InputDeviceStates m_State;
    };
} // namespace Fling
