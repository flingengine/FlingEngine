#pragma once

#include "FlingTypes.h"
#include "Input/InputMessage.h"
#include <limits>
#include <vector>

namespace Fling
{
    /**
     * A fixed-size ring of InputMessages with one producer and any number of
     * consumers (InputMessageProcessor). Push never waits: when the ring is full, it
     * overwrites the oldest message. Consumers copy messages out by sequence number,
     * so a later version can use a seqlock for threads with the same API.
     *
     * Today everything runs on one thread.
     */
    class InputMessageQueue
    {
    public:
        static constexpr uint32 DefaultCapacity = 4096;

        explicit InputMessageQueue(uint32 t_Capacity = DefaultCapacity);

        /**
         * Add a message. Its Sequence must be 1 more than the last pushed message (the
         * first message can have any Sequence above 0). InputMessageHandler sets it.
         */
        void Push(const InputMessage& t_Message);

        /** Sequence of the newest message, or 0 if the queue is empty. */
        uint64 NewestSequence() const { return m_Newest; }

        /** Sequence of the oldest message that is still in the ring, or 0 if the queue is empty. */
        uint64 OldestSequence() const;

        uint32 Capacity() const { return static_cast<uint32>(m_Slots.size()); }

        /** Copy the message with this sequence. Returns false if it is not in the ring. */
        bool TryGet(uint64 t_Sequence, InputMessage& t_Out) const;

        /**
         * Copy messages with a Sequence after t_AfterSequence, in order, into t_Out, up to
         * t_Max. Stops at the first message with TimeNs >= t_UntilNs. If messages after
         * t_AfterSequence were overwritten, it copies nothing and returns 0 (the caller
         * checks OldestSequence to find out).
         *
         * @return The number of messages copied
         */
        uint32 CopyRange(uint64 t_AfterSequence,
                         InputMessage* t_Out,
                         uint32 t_Max,
                         int64 t_UntilNs = std::numeric_limits<int64>::max()) const;

    private:
        std::vector<InputMessage> m_Slots;

        uint64 m_Newest = 0;

        /** The number of messages pushed, up to the capacity. */
        uint64 m_Count = 0;
    };
} // namespace Fling
