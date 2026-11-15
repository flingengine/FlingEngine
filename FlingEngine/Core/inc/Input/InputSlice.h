#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Input/InputCut.h"
#include "Input/InputKey.h"
#include "Input/InputMessage.h"

#include <cstddef>
#include <span>
#include <vector>

namespace Fling
{
    /**
     * One mouse move message: its sequence and its relative counts. It has no time: the
     * simulation reads it, and its results must not depend on wall clock time.
     */
    struct InputMouseSample
    {
        uint64 Sequence = 0;
        int32 DeltaX = 0;
        int32 DeltaY = 0;

        bool operator==(const InputMouseSample&) const = default;
    };

    /** One key or button press or release message: its sequence, the key, and the edge. It has no time. */
    struct InputKeyEvent
    {
        uint64 Sequence = 0;
        InputKey Key;

        /** True for KeyDown, false for KeyUp. */
        bool Down = false;

        bool operator==(const InputKeyEvent&) const = default;
    };

    /**
     * The gameplay input of one range of the message queue (Cut), in sequence order: the
     * mouse moves and the key edges that UI capture did not take. It is the input of a
     * fixed tick (TickInput::Slice) and the pending input of a frame (PendingInput), made
     * by the same Append from the same messages. So a game rule that takes an InputSlice
     * gives the frame side exactly what the next ticks will make from the same input.
     */
    struct CORE_API InputSlice
    {
        /** The range of the queue that was read. Messages of other types in it are not listed. */
        InputCut Cut;

        /** Each mouse move of the range, in sequence order. */
        std::vector<InputMouseSample> MouseSamples;

        /** Each key and button press and release of the range, in sequence order. */
        std::vector<InputKeyEvent> KeyEvents;

        /**
         * True if the queue overwrote messages before they were read. Both sides then drop
         * every message that was in the queue: the tick goes on from the newest state
         * (TickInputBuilder), and the frame side shows no pending input
         * (PendingInputReader). A tick with it is marked in the ledger
         * (TickRecord::InputOverran), and a replay stops with an error.
         */
        bool Overran = false;

        /** The sums of MouseSamples, in integer counts. */
        int32 MouseDeltaX() const;
        int32 MouseDeltaY() const;

        /** The number of presses of the key in the range. */
        uint32 PressCount(InputKey t_Key) const;

        bool WasPressed(InputKey t_Key) const { return PressCount(t_Key) > 0; }

        /**
         * Add one message: a mouse move or a key edge that capture does not take goes to
         * MouseSamples or KeyEvents. Other messages add nothing. The capture is the
         * message's own (InputMessage::Capture) and t_Forced.
         */
        void Append(const InputMessage& t_Message, InputCapture t_Forced);

        /** Empties it and keeps the memory of the lists. */
        void Clear();
    };

    /**
     * Calls t_OnMouse(sample) and t_OnKey(event) for each item of the two lists, in
     * sequence order: one walk over the input stream, with mouse moves and key edges at
     * their own places. Both lists must be in sequence order (InputSlice keeps them so). A
     * game uses it to apply a key edge at its position among the mouse samples.
     */
    template <typename OnMouse, typename OnKey>
    void ForEachInOrder(const std::span<const InputMouseSample> t_Samples,
                        const std::span<const InputKeyEvent> t_Keys,
                        OnMouse&& t_OnMouse,
                        OnKey&& t_OnKey)
    {
        std::size_t m = 0;
        std::size_t k = 0;
        while (m < t_Samples.size() || k < t_Keys.size())
        {
            if (k == t_Keys.size() || (m < t_Samples.size() && t_Samples[m].Sequence < t_Keys[k].Sequence))
            {
                t_OnMouse(t_Samples[m++]);
            }
            else
            {
                t_OnKey(t_Keys[k++]);
            }
        }
    }
} // namespace Fling
