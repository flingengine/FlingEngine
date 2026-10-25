#include "Input/InputMessageProcessor.h"
#include "Input/InputMessageQueue.h"
#include <algorithm>

namespace Fling
{
    namespace
    {
        constexpr uint32 ChunkSize = 64;
    } // namespace

    InputMessageProcessor::InputMessageProcessor(std::weak_ptr<const InputMessageQueue> t_Queue, uint64 t_StartAfter)
        : m_Queue(std::move(t_Queue)), m_Cursor(t_StartAfter)
    {
    }

    uint32 InputMessageProcessor::Read(InputMessage* t_Out, uint32 t_Max, int64 t_UntilNs) const
    {
        const std::shared_ptr<const InputMessageQueue> Queue = m_Queue.lock();
        return Queue ? Queue->CopyRange(m_Cursor, t_Out, t_Max, t_UntilNs) : 0;
    }

    void InputMessageProcessor::Consume(uint64 t_Sequence)
    {
        const std::shared_ptr<const InputMessageQueue> Queue = m_Queue.lock();
        if (!Queue)
        {
            return;
        }
        t_Sequence = std::min(t_Sequence, Queue->NewestSequence());

        InputMessage Chunk[ChunkSize];
        while (m_Cursor < t_Sequence)
        {
            const uint64 Remaining = t_Sequence - m_Cursor;
            const uint32 Want = static_cast<uint32>(std::min<uint64>(Remaining, ChunkSize));
            const uint32 Got = Queue->CopyRange(m_Cursor, Chunk, Want);
            if (Got == 0)
            {
                // Messages after the cursor are gone. Apply nothing past the gap.
                m_Overran = true;
                return;
            }
            for (uint32 i = 0; i < Got; ++i)
            {
                m_State.Apply(Chunk[i]);
                m_Cursor = Chunk[i].Sequence;
            }
        }
    }

    uint32 InputMessageProcessor::ConsumeUntil(int64 t_UntilNs)
    {
        const std::shared_ptr<const InputMessageQueue> Queue = m_Queue.lock();
        if (!Queue)
        {
            return 0;
        }

        uint32 Total = 0;
        InputMessage Chunk[ChunkSize];
        while (true)
        {
            const uint32 Got = Queue->CopyRange(m_Cursor, Chunk, ChunkSize, t_UntilNs);
            if (Got == 0)
            {
                if (Overran())
                {
                    m_Overran = true;
                }
                break;
            }
            for (uint32 i = 0; i < Got; ++i)
            {
                m_State.Apply(Chunk[i]);
                m_Cursor = Chunk[i].Sequence;
            }
            Total += Got;
            if (Got < ChunkSize)
            {
                break;
            }
        }
        return Total;
    }

    bool InputMessageProcessor::Overran() const
    {
        if (m_Overran)
        {
            return true;
        }
        const std::shared_ptr<const InputMessageQueue> Queue = m_Queue.lock();
        if (!Queue)
        {
            return false;
        }
        return Queue->NewestSequence() > m_Cursor && Queue->OldestSequence() > m_Cursor + 1;
    }

    void InputMessageProcessor::Resync(const InputDeviceStates& t_Latest, uint64 t_LatestSequence)
    {
        int64 TimeNs = 0;
        InputMessage Latest;
        const std::shared_ptr<const InputMessageQueue> Queue = m_Queue.lock();
        if (Queue && Queue->TryGet(t_LatestSequence, Latest))
        {
            TimeNs = Latest.TimeNs;
        }

        for (const InputMessage& Message : m_State.MakeTransitionMessages(t_Latest, TimeNs))
        {
            m_State.Apply(Message);
        }
        m_Cursor = t_LatestSequence;
        m_Overran = false;
    }
} // namespace Fling
