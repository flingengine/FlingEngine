#pragma once

#include "FlingTypes.h"
#include "Input/InputMessage.h"

#include <atomic>
#include <limits>
#include <memory>

namespace Fling
{
    /**
     * A fixed-size ring of InputMessages with one writer thread and any number of
     * readers (InputMessageProcessor, PendingInputReader) on any threads. Push never
     * waits: when the ring is full, it overwrites the oldest message. Readers copy
     * messages out by sequence number and never block the writer.
     *
     * Each slot is a seqlock: the writer marks the slot busy, writes the message, then
     * stores its sequence (release). A reader loads the sequence (acquire), copies the
     * message, and loads the sequence again. If either load is not the sequence it
     * wants, the writer overwrote the slot: the reader stops there and treats the rest
     * as lost (an overrun). The message is copied as 8-byte words through
     * std::atomic_ref, so a copy that races with the writer is not a data race, and a
     * torn copy is always found and dropped.
     *
     * Only the writer thread may call Push and PublishWatermark. Every other function
     * is safe on any thread.
     */
    class InputMessageQueue
    {
    public:
        static constexpr uint32 DefaultCapacity = 4096;

        explicit InputMessageQueue(uint32 t_Capacity = DefaultCapacity);

        /**
         * Writer: add a message. Its Sequence must be 1 more than the last pushed message
         * (the first message can have any Sequence above 0). InputMessageHandler sets it.
         */
        void Push(const InputMessage& t_Message);

        /** Sequence of the newest message that is completely written, or 0 if the queue is empty. */
        uint64 NewestSequence() const { return m_Newest.load(std::memory_order_acquire); }

        /**
         * Sequence of the oldest message that can still be in the ring, or 0 if the queue
         * is empty. With a writer on another thread, that slot can be overwritten at any
         * time after this returns, so a copy checks it again.
         */
        uint64 OldestSequence() const;

        uint32 Capacity() const { return m_Capacity; }

        /** Copy the message with this sequence. False if it is not in the ring, or it was overwritten during the copy.
         */
        bool TryGet(uint64 t_Sequence, InputMessage& t_Out) const;

        /**
         * Copy messages with a Sequence after t_AfterSequence, in order, into t_Out, up to
         * t_Max. Copies only messages that the writer has completely written. Stops at the
         * first message with TimeNs >= t_UntilNs. If the message after t_AfterSequence was
         * overwritten, it copies nothing and returns 0 (the caller checks OldestSequence to
         * find out). If a later one is overwritten during the copy, it returns the messages
         * before it, and the next call finds the overrun.
         *
         * @return The number of messages copied
         */
        uint32 CopyRange(uint64 t_AfterSequence,
                         InputMessage* t_Out,
                         uint32 t_Max,
                         int64 t_UntilNs = std::numeric_limits<int64>::max()) const;

        /**
         * Writer: every message with a TimeNs before t_TimeNs is in the queue now. A tick
         * whose window ends at or before the watermark has all of its input. The
         * watermark never goes back.
         */
        void PublishWatermark(int64 t_TimeNs);

        /** The newest watermark, or the lowest int64 before the first. */
        int64 Watermark() const { return m_Watermark.load(std::memory_order_acquire); }

    private:
        static constexpr uint32 WordCount = static_cast<uint32>((sizeof(InputMessage) + 7) / 8);

        /** The sequence of a slot while the writer writes it. No message has it. */
        static constexpr uint64 BusySequence = std::numeric_limits<uint64>::max();

        struct Slot
        {
            std::atomic<uint64> Sequence{0};
            uint64 Words[WordCount] = {};
        };

        /** Copy the message t_Sequence from its slot. False if the slot holds another message, or changed during the
         * copy. */
        bool ReadSlot(uint64 t_Sequence, InputMessage& t_Out) const;

        uint32 m_Capacity = 1;

        std::unique_ptr<Slot[]> m_Slots;

        /** The first sequence pushed. The writer stores it before the first m_Newest store, so a reader that sees
         * m_Newest sees it. */
        std::atomic<uint64> m_First{0};

        /** Every slot up to this sequence is completely written. Readers load it with acquire. */
        alignas(64) std::atomic<uint64> m_Newest{0};

        std::atomic<int64> m_Watermark{std::numeric_limits<int64>::min()};
    };
} // namespace Fling
