#include "Input/InputMessageHandler.h"
#include "Input/TickInput.h"
#include "Strings/String.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace Fling
{
    namespace
    {
        /** The entry of t_Key in t_Keys, added if it is not there. */
        TickKeyState& EntryOf(std::vector<std::pair<InputKey, TickKeyState>>& t_Keys, const InputKey t_Key)
        {
            auto It = std::find_if(
                t_Keys.begin(), t_Keys.end(), [t_Key](const auto& t_Entry) { return t_Entry.first == t_Key; });
            if (It == t_Keys.end())
            {
                return t_Keys.emplace_back(t_Key, TickKeyState{}).second;
            }
            return It->second;
        }

        void AddOne(uint16& t_Count)
        {
            t_Count = t_Count < std::numeric_limits<uint16>::max() ? static_cast<uint16>(t_Count + 1) : t_Count;
        }

        InputMessageProcessor MakeProcessor(const std::shared_ptr<const InputMessageHandler>& t_Handler)
        {
            if (t_Handler)
            {
                return t_Handler->CreateProcessor();
            }
            return InputMessageProcessor(std::weak_ptr<const InputMessageQueue>{}, 0);
        }
    } // namespace

    TickKeyState TickInput::GetKeyState(const InputKey t_Key) const
    {
        for (const auto& Entry : Keys)
        {
            if (Entry.first == t_Key)
            {
                return Entry.second;
            }
        }
        return TickKeyState{};
    }

    void TickInput::Clear()
    {
        Slice.Clear();
        Keys.clear();
        Capture = InputCapture::None;
    }

    TickInputBuilder::TickInputBuilder(std::shared_ptr<const InputMessageHandler> t_Handler)
        : m_Handler(t_Handler), m_Processor(MakeProcessor(t_Handler))
    {
    }

    void TickInputBuilder::Build(TickInput& t_Out, const TickInputLimit& t_Limit, const InputCapture t_Forced)
    {
        t_Out.Clear();
        t_Out.Slice.Cut = InputCut{m_Processor.GetCursor(), m_Processor.GetCursor()};

        const std::shared_ptr<const InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler || !m_Processor.IsValid())
        {
            return;
        }

        // A long stop (for example a breakpoint) can overrun the queue. Held keys stay
        // right, and the taps in the lost range are gone.
        if (m_Processor.Overran())
        {
            Resync(*Handler);
            t_Out.Slice.Overran = true;
        }

        TakeMessages(t_Out, t_Forced, t_Limit);
        t_Out.Slice.Cut.LastSequence = m_Processor.GetCursor();
        t_Out.Capture = m_Capture;
        MergeKeys(t_Out, m_Capture | t_Forced);
        m_Processor.ClearEdges();
    }

    void TickInputBuilder::Resync(const InputMessageHandler& t_Handler)
    {
        const InputStateSnapshot Latest = t_Handler.GetLatestSnapshot();
        m_Processor.Resync(Latest.States, Latest.Sequence);
        m_Capture = Latest.Capture;
    }

    void TickInputBuilder::TakeMessages(TickInput& t_Out, const InputCapture t_Forced, const TickInputLimit& t_Limit)
    {
        constexpr uint32 ChunkSize = 64;
        InputMessage Chunk[ChunkSize];
        while (true)
        {
            // Times never decrease, so the time limit stops at the first late message
            uint32 Got = m_Processor.Read(Chunk, ChunkSize, t_Limit.UntilNs);
            while (Got > 0 && Chunk[Got - 1].Sequence > t_Limit.UntilSequence)
            {
                --Got;
            }
            // One copy for the slice and the state, so the slice never holds a message after the cut
            for (uint32 i = 0; i < Got; ++i)
            {
                t_Out.Slice.Append(Chunk[i], t_Forced);
                m_Processor.Apply(Chunk[i]);
                m_Capture = Chunk[i].Capture;
            }
            if (Got < ChunkSize)
            {
                break;
            }
        }
    }

    void TickInputBuilder::MergeKeys(TickInput& t_Out, const InputCapture t_Capture) const
    {
        // Held and axis value: the state at the cut, on any device
        for (const auto& Device : m_Processor.GetState().GetDevices())
        {
            for (const auto& [Key, State] : Device.second.Keys)
            {
                if (IsKeyCaptured(t_Capture, Key))
                {
                    continue;
                }
                TickKeyState& Into = EntryOf(t_Out.Keys, Key);
                Into.IsHeld = Into.IsHeld || State.IsHeld;
                // The device map has no fixed order, and it differs between standard libraries: the rule must not
                // depend on it. The largest magnitude wins; on a tie the larger value, and +0 over -0.
                const float Size = std::fabs(State.Value);
                const float IntoSize = std::fabs(Into.Value);
                const bool Wins =
                    Size > IntoSize || (Size == IntoSize && (State.Value > Into.Value ||
                                                             (!std::signbit(State.Value) && std::signbit(Into.Value))));
                if (Wins)
                {
                    Into.Value = State.Value;
                }
            }
        }

        // Presses and releases: the slice's key edges, so there is one rule for "pressed this tick"
        for (const InputKeyEvent& Edge : t_Out.Slice.KeyEvents)
        {
            TickKeyState& Into = EntryOf(t_Out.Keys, Edge.Key);
            AddOne(Edge.Down ? Into.PressCount : Into.ReleaseCount);
        }

        // The device map has no fixed order. Sort the keys by name, then by key for two keys with one name, so that
        // two runs, and two platforms, with the same input give the same TickInput.
        std::sort(t_Out.Keys.begin(),
                  t_Out.Keys.end(),
                  [](const auto& t_A, const auto& t_B)
                  {
                      const StringView NameA(t_A.first.GetName());
                      const StringView NameB(t_B.first.GetName());
                      return NameA != NameB ? NameA < NameB : t_A.first < t_B.first;
                  });
    }
} // namespace Fling
