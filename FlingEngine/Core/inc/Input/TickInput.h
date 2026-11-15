#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Input/InputKey.h"
#include "Input/InputMessageProcessor.h"
#include "Input/InputSlice.h"

#include <limits>
#include <memory>
#include <utility>
#include <vector>

namespace Fling
{
    class InputMessageHandler;

    /**
     * The state of one key in one fixed tick, merged over every device. The counts are
     * the key's presses and releases in TickInput::Slice.KeyEvents (the one rule for
     * "pressed this tick", which the frame side's InputSlice uses too), so a press and a
     * release inside one tick give PressCount 1, ReleaseCount 1, and IsHeld false: no
     * press is lost. It has no time.
     */
    struct TickKeyState
    {
        bool IsHeld = false;

        uint16 PressCount = 0;

        uint16 ReleaseCount = 0;

        /** Axis value, or 1 while a button is held and 0 when it is not. */
        float Value = 0.0f;

        bool WasPressed() const { return PressCount > 0; }

        bool WasReleased() const { return ReleaseCount > 0; }
    };

    /**
     * The input of one fixed simulation tick: its InputSlice (the cut it consumed, the
     * mouse samples and key edges in order), and the state of each key at the end of it.
     * Nothing in it is a wall clock time, so a fixed system that reads it cannot depend
     * on one.
     */
    struct CORE_API TickInput
    {
        /** The messages this tick consumed. Slice.Cut is the tick's cut: the frame side reads the input after it. */
        InputSlice Slice;

        /** One entry for each key that has state (held, an edge, or an axis value), sorted by key name. */
        std::vector<std::pair<InputKey, TickKeyState>> Keys;

        /** The UI capture at the cut: the Capture of the message at Slice.Cut.LastSequence. */
        InputCapture Capture = InputCapture::None;

        /** The merged state of one key: held on any device, edge counts summed, the largest axis value. */
        TickKeyState GetKeyState(InputKey t_Key) const;

        bool IsHeld(InputKey t_Key) const { return GetKeyState(t_Key).IsHeld; }

        bool WasPressed(InputKey t_Key) const { return GetKeyState(t_Key).WasPressed(); }

        bool WasReleased(InputKey t_Key) const { return GetKeyState(t_Key).WasReleased(); }

        float GetAxisValue(InputKey t_Key) const { return GetKeyState(t_Key).Value; }

        /** Clears the input of the tick, its cut and capture too. */
        void Clear();
    };

    /** Where one TickInputBuilder::Build stops. The default takes every message in the queue. */
    struct TickInputLimit
    {
        /** Take only messages with TimeNs before this: the end of the tick's window (time rule). */
        int64 UntilNs = std::numeric_limits<int64>::max();

        /** Take only messages up to and including this sequence: a recorded cut (replay rule). */
        uint64 UntilSequence = std::numeric_limits<uint64>::max();
    };

    /**
     * Builds one TickInput for each fixed tick that runs. It has its own processor
     * (cursor) on the input queue, separate from every other reader.
     * TickInputProvider decides the limit of each tick; use the builder directly only to
     * test the limits.
     *
     * Each Build takes the messages that no earlier tick took, in sequence order, up to
     * the limit: with the time rule, every message before the end of the tick's window
     * (the first message at or after it stops the walk, so nothing is reordered); with a
     * recorded cut, every message up to that sequence. It takes only messages that the
     * writer has completely written. A message that comes in after its tick ran goes to
     * the next tick. A frame that runs no tick keeps its messages for the next tick that
     * runs.
     *
     * UI capture comes from the stream: each message carries the capture that applied
     * when it was written (InputMessage::Capture), and InputSlice::Append drops exactly
     * the messages that PendingInputReader drops. A captured press or release is not in
     * Slice.KeyEvents and so not in the counts of Keys. The held state uses the capture at
     * the cut: a key pressed under capture is held, with no press, once the capture ends.
     *
     * After an overrun (the queue overwrote messages that no tick took) the builder goes on
     * from the newest state: held keys are right, and the presses and moves in the lost
     * range are gone. Slice.Overran is set on that tick.
     *
     * One thread at a time: the thread that runs the ticks. It can be another thread
     * than the writer of the queue.
     */
    class CORE_API TickInputBuilder
    {
    public:
        explicit TickInputBuilder(std::shared_ptr<const InputMessageHandler> t_Handler);

        /**
         * Consume the new messages up to t_Limit, write the tick's input into t_Out, then
         * clear the edges. A captured keyboard or mouse (UI owns it) gives neutral input
         * for its keys.
         *
         * @param t_Forced  Treat these devices as captured too, whatever the stream says (tests and tools)
         */
        void Build(TickInput& t_Out, const TickInputLimit& t_Limit = {}, InputCapture t_Forced = InputCapture::None);

        /** The sequence of the last consumed message: the cut of the last Build. */
        uint64 GetCursor() const { return m_Processor.GetCursor(); }

    private:
        std::weak_ptr<const InputMessageHandler> m_Handler;
        InputMessageProcessor m_Processor;

        /** Capture of the last consumed message. Kept for a tick that consumes nothing. */
        InputCapture m_Capture = InputCapture::None;

        /** After an overrun, continue from the newest state. */
        void Resync(const InputMessageHandler& t_Handler);

        /** Append the messages up to the limit to t_Out.Slice, and apply the same copies to the processor. */
        void TakeMessages(TickInput& t_Out, InputCapture t_Forced, const TickInputLimit& t_Limit);

        /**
         * Fill t_Out.Keys, sorted by key name: the held state and axis value of every device
         * at the cut (without the keys that t_Capture owns), and the counts from
         * t_Out.Slice.KeyEvents.
         */
        void MergeKeys(TickInput& t_Out, InputCapture t_Capture) const;
    };
} // namespace Fling
