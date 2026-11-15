#include "Input/InputMessageHandler.h"
#include "Input/PendingInputReader.h"

namespace Fling
{
    PendingInputReader::PendingInputReader(std::shared_ptr<const InputMessageHandler> t_Handler)
        : m_Handler(std::move(t_Handler))
    {
    }

    void PendingInputReader::Read(const uint64 t_AfterSequence, InputSlice& t_Out, const InputCapture t_Forced) const
    {
        t_Out.Clear();
        t_Out.Cut = InputCut{t_AfterSequence, t_AfterSequence};

        const std::shared_ptr<const InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler)
        {
            return;
        }
        const InputMessageQueue& Queue = Handler->GetQueue();

        uint64 After = t_AfterSequence;
        if (Queue.NewestSequence() > After && Queue.OldestSequence() > After + 1)
        {
            // The next tick drops what is in the queue now (it resyncs to the newest state),
            // so the frame shows none of it: never input that no tick applies
            t_Out.Overran = true;
            t_Out.Cut.LastSequence = Queue.NewestSequence();
            return;
        }

        constexpr uint32 ChunkSize = 64;
        InputMessage Chunk[ChunkSize];
        while (true)
        {
            const uint32 Got = Queue.CopyRange(After, Chunk, ChunkSize);
            for (uint32 i = 0; i < Got; ++i)
            {
                t_Out.Append(Chunk[i], t_Forced);
                After = Chunk[i].Sequence;
            }
            if (Got < ChunkSize)
            {
                break;
            }
        }
        t_Out.Cut.LastSequence = After;
    }
} // namespace Fling
