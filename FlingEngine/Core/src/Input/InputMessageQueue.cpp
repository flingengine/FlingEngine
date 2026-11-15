#include "Input/InputMessageQueue.h"

#include <algorithm>
#include <cassert>
#include <cstring>

namespace Fling
{
    InputMessageQueue::InputMessageQueue(uint32 t_Capacity)
        : m_Capacity(std::max<uint32>(t_Capacity, 1)), m_Slots(std::make_unique<Slot[]>(m_Capacity))
    {
    }

    void InputMessageQueue::Push(const InputMessage& t_Message)
    {
        const uint64 Newest = m_Newest.load(std::memory_order_relaxed);
        const bool IsValid = (Newest == 0) ? (t_Message.Sequence > 0) : (t_Message.Sequence == Newest + 1);
        assert(IsValid && "InputMessageQueue::Push: the sequence is not newest + 1");
        if (!IsValid)
        {
            return;
        }
        if (Newest == 0)
        {
            m_First.store(t_Message.Sequence, std::memory_order_relaxed);
        }

        uint64 Words[WordCount] = {};
        std::memcpy(Words, &t_Message, sizeof(InputMessage));

        // Seqlock write: mark busy, then the data, then the sequence. The release fence
        // keeps the data stores after the busy mark for a reader that sees the data.
        Slot& Target = m_Slots[t_Message.Sequence % m_Capacity];
        Target.Sequence.store(BusySequence, std::memory_order_relaxed);
        std::atomic_thread_fence(std::memory_order_release);
        for (uint32 i = 0; i < WordCount; ++i)
        {
            std::atomic_ref<uint64>(Target.Words[i]).store(Words[i], std::memory_order_relaxed);
        }
        Target.Sequence.store(t_Message.Sequence, std::memory_order_release);
        m_Newest.store(t_Message.Sequence, std::memory_order_release);
    }

    bool InputMessageQueue::ReadSlot(const uint64 t_Sequence, InputMessage& t_Out) const
    {
        Slot& Source = m_Slots[t_Sequence % m_Capacity];
        if (Source.Sequence.load(std::memory_order_acquire) != t_Sequence)
        {
            return false;
        }
        uint64 Words[WordCount];
        for (uint32 i = 0; i < WordCount; ++i)
        {
            Words[i] = std::atomic_ref<uint64>(Source.Words[i]).load(std::memory_order_relaxed);
        }
        // The data loads must happen before the second sequence load
        std::atomic_thread_fence(std::memory_order_acquire);
        if (Source.Sequence.load(std::memory_order_relaxed) != t_Sequence)
        {
            return false;
        }
        std::memcpy(&t_Out, Words, sizeof(InputMessage));
        return true;
    }

    uint64 InputMessageQueue::OldestSequence() const
    {
        const uint64 Newest = NewestSequence();
        if (Newest == 0)
        {
            return 0;
        }
        const uint64 First = m_First.load(std::memory_order_relaxed);
        return Newest - First + 1 > m_Capacity ? Newest - m_Capacity + 1 : First;
    }

    bool InputMessageQueue::TryGet(uint64 t_Sequence, InputMessage& t_Out) const
    {
        const uint64 Newest = NewestSequence();
        if (Newest == 0 || t_Sequence < OldestSequence() || t_Sequence > Newest)
        {
            return false;
        }
        return ReadSlot(t_Sequence, t_Out);
    }

    uint32
    InputMessageQueue::CopyRange(uint64 t_AfterSequence, InputMessage* t_Out, uint32 t_Max, int64 t_UntilNs) const
    {
        const uint64 Newest = NewestSequence();
        if (Newest == 0 || t_AfterSequence >= Newest || t_Max == 0)
        {
            return 0;
        }

        // The next message is older than the ring, so some messages were overwritten.
        const uint64 First = t_AfterSequence + 1;
        if (First < OldestSequence())
        {
            return 0;
        }

        uint32 Copied = 0;
        for (uint64 Seq = First; Seq <= Newest && Copied < t_Max; ++Seq)
        {
            InputMessage& Message = t_Out[Copied];
            if (!ReadSlot(Seq, Message) || Message.TimeNs >= t_UntilNs)
            {
                break;
            }
            ++Copied;
        }
        return Copied;
    }

    void InputMessageQueue::PublishWatermark(const int64 t_TimeNs)
    {
        if (t_TimeNs > m_Watermark.load(std::memory_order_relaxed))
        {
            m_Watermark.store(t_TimeNs, std::memory_order_release);
        }
    }
} // namespace Fling
