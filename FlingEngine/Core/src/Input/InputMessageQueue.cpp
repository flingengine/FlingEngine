#include "Input/InputMessageQueue.h"
#include <algorithm>
#include <cassert>

namespace Fling
{
    InputMessageQueue::InputMessageQueue(uint32 t_Capacity) : m_Slots(std::max<uint32>(t_Capacity, 1))
    {
    }

    void InputMessageQueue::Push(const InputMessage& t_Message)
    {
        const bool IsValid = (m_Newest == 0) ? (t_Message.Sequence > 0) : (t_Message.Sequence == m_Newest + 1);
        assert(IsValid && "InputMessageQueue::Push: the sequence is not newest + 1");
        if (!IsValid)
        {
            return;
        }

        m_Slots[t_Message.Sequence % m_Slots.size()] = t_Message;
        m_Newest = t_Message.Sequence;
        if (m_Count < m_Slots.size())
        {
            ++m_Count;
        }
    }

    uint64 InputMessageQueue::OldestSequence() const
    {
        if (m_Count == 0)
        {
            return 0;
        }
        return m_Newest - m_Count + 1;
    }

    bool InputMessageQueue::TryGet(uint64 t_Sequence, InputMessage& t_Out) const
    {
        if (m_Count == 0 || t_Sequence < OldestSequence() || t_Sequence > m_Newest)
        {
            return false;
        }
        t_Out = m_Slots[t_Sequence % m_Slots.size()];
        return true;
    }

    uint32
    InputMessageQueue::CopyRange(uint64 t_AfterSequence, InputMessage* t_Out, uint32 t_Max, int64 t_UntilNs) const
    {
        if (m_Count == 0 || t_AfterSequence >= m_Newest || t_Max == 0)
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
        for (uint64 Seq = First; Seq <= m_Newest && Copied < t_Max; ++Seq)
        {
            const InputMessage& Slot = m_Slots[Seq % m_Slots.size()];
            if (Slot.TimeNs >= t_UntilNs)
            {
                break;
            }
            t_Out[Copied++] = Slot;
        }
        return Copied;
    }
} // namespace Fling
