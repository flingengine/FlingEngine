#include "Input/InputMessageHandler.h"
#include "Strings/String.h"
#include "Systems/InputSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Fling
{
    namespace
    {
        uint16 AddCounts(const uint16 t_A, const uint16 t_B)
        {
            const uint32 Sum = static_cast<uint32>(t_A) + t_B;
            return static_cast<uint16>(std::min<uint32>(Sum, std::numeric_limits<uint16>::max()));
        }

        /** Merge one device's key state into the snapshot's state for that key. */
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
    } // namespace

    void InputSystem::FixedUpdate(Context& ctx) const
    {
        InputSource& Source = ctx.Singleton<InputSource>();
        InputSnapshot& Snapshot = ctx.Singleton<InputSnapshot>();

        Snapshot.Keys.clear();
        Snapshot.MouseDeltaX = 0;
        Snapshot.MouseDeltaY = 0;

        const std::shared_ptr<const InputMessageHandler> Handler = Source.Handler.lock();
        if (!Handler || !Source.Reader || !Source.Reader->IsValid())
        {
            return;
        }
        InputMessageProcessor& Reader = *Source.Reader;

        // TODO: In the future when we have a high polling rate input thread (or input is polled on 
        // the presentation thread itself) we will probably want the fixed tick input to only consume input 
        // up to a specific point in time of the simuation, to get accurate timing and frame correct inputs.
        // i.e. we may poll input on the presentation thread ahead of the actual sim time ("predictive" cameras)
        // so we will have to reconcile that. We would do that here in the future maybe, possibly even having multiple 
        // input systems. one for fixed tick sim and one for variable render, or two different states that are populated
        // at different times within this system.          

        // A long stop (for example a breakpoint) can overrun the queue. Held keys stay
        // right, and the taps in the lost range are gone.
        if (Reader.Overran())
        {
            Reader.Resync(Handler->GetLatestState(), Handler->GetLatestSequence());
        }
        Reader.ConsumeUntil();

        for (const auto& Device : Reader.GetState().GetDevices())
        {
            for (const auto& [Key, State] : Device.second.Keys)
            {
                if ((Source.KeyboardCaptured && Key.IsKeyboardKey()) || (Source.MouseCaptured && Key.IsMouseButton()))
                {
                    continue;
                }

                auto It = std::find_if(Snapshot.Keys.begin(),
                                       Snapshot.Keys.end(),
                                       [Key](const auto& t_Entry) { return t_Entry.first == Key; });
                if (It == Snapshot.Keys.end())
                {
                    Snapshot.Keys.emplace_back(Key, State);
                }
                else
                {
                    Merge(It->second, State);
                }
            }

            if (!Source.MouseCaptured)
            {
                Snapshot.MouseDeltaX += Device.second.MouseDeltaX;
                Snapshot.MouseDeltaY += Device.second.MouseDeltaY;
            }
        }

        // The device map has no fixed order. Sort the keys by name, so that two runs
        // with the same input give the same snapshot.
        // TODO: These key names should really be Fling::Name types, not actual strings. ouch
        std::sort(Snapshot.Keys.begin(),
                  Snapshot.Keys.end(),
                  [](const auto& t_A, const auto& t_B)
                  { return StringView(t_A.first.GetName()) < StringView(t_B.first.GetName()); });

        Reader.ClearEdges();
    }
} // namespace Fling
