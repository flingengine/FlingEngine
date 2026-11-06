#include "Input/InputMessageHandler.h"
#include "Input/TickInput.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <string_view>

namespace Fling
{
    namespace
    {
        uint16 AddCounts(const uint16 t_A, const uint16 t_B)
        {
            const uint32 Sum = static_cast<uint32>(t_A) + t_B;
            return static_cast<uint16>(std::min<uint32>(Sum, std::numeric_limits<uint16>::max()));
        }

        /** Merge one device's key state into the tick's state for that key. */
        void Merge(InputKeyState& t_Into, const InputKeyState& t_From)
        {
            t_Into.IsHeld = t_Into.IsHeld || t_From.IsHeld;
            t_Into.PressCount = AddCounts(t_Into.PressCount, t_From.PressCount);
            t_Into.ReleaseCount = AddCounts(t_Into.ReleaseCount, t_From.ReleaseCount);
            t_Into.RepeatCount = AddCounts(t_Into.RepeatCount, t_From.RepeatCount);
            if (std::fabs(t_From.Value) > std::fabs(t_Into.Value))
            {
                t_Into.Value = t_From.Value;
            }
            t_Into.LastChangeTimeNs = std::max(t_Into.LastChangeTimeNs, t_From.LastChangeTimeNs);
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

    InputKeyState TickInput::GetKeyState(const InputKey t_Key) const
    {
        for (const auto& Entry : Keys)
        {
            if (Entry.first == t_Key)
            {
                return Entry.second;
            }
        }
        return InputKeyState{};
    }

    void TickInput::Clear()
    {
        Keys.clear();
        MouseDeltaX = 0;
        MouseDeltaY = 0;
    }

    TickInputBuilder::TickInputBuilder(std::shared_ptr<const InputMessageHandler> t_Handler)
        : m_Handler(t_Handler), m_Processor(MakeProcessor(t_Handler))
    {
    }

    void TickInputBuilder::Build(TickInput& t_Out, const bool t_KeyboardCaptured, const bool t_MouseCaptured)
    {
        t_Out.Clear();

        const std::shared_ptr<const InputMessageHandler> Handler = m_Handler.lock();
        if (!Handler || !m_Processor.IsValid())
        {
            return;
        }

        // A long stop (for example a breakpoint) can overrun the queue. Held keys stay
        // right, and the taps in the lost range are gone.
        if (m_Processor.Overran())
        {
            m_Processor.Resync(Handler->GetLatestState(), Handler->GetLatestSequence());
        }
        m_Processor.ConsumeUntil();

        for (const auto& Device : m_Processor.GetState().GetDevices())
        {
            for (const auto& [Key, State] : Device.second.Keys)
            {
                if ((t_KeyboardCaptured && Key.IsKeyboardKey()) || (t_MouseCaptured && Key.IsMouseButton()))
                {
                    continue;
                }

                auto It = std::find_if(
                    t_Out.Keys.begin(), t_Out.Keys.end(), [Key](const auto& t_Entry) { return t_Entry.first == Key; });
                if (It == t_Out.Keys.end())
                {
                    t_Out.Keys.emplace_back(Key, State);
                }
                else
                {
                    Merge(It->second, State);
                }
            }

            if (!t_MouseCaptured)
            {
                t_Out.MouseDeltaX += Device.second.MouseDeltaX;
                t_Out.MouseDeltaY += Device.second.MouseDeltaY;
            }
        }

        // The device map has no fixed order. Sort the keys by name, so that two runs
        // with the same input give the same TickInput.
        std::sort(t_Out.Keys.begin(),
                  t_Out.Keys.end(),
                  [](const auto& t_A, const auto& t_B)
                  { return std::string_view(t_A.first.GetName()) < std::string_view(t_B.first.GetName()); });

        m_Processor.ClearEdges();
    }
} // namespace Fling
