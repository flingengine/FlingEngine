#pragma once

#include "CoreAPI.h"
#include "FlingTypes.h"
#include "Input/InputKey.h"
#include "Input/InputKeyState.h"
#include "Input/InputMessageProcessor.h"

#include <memory>
#include <utility>
#include <vector>

namespace Fling
{
    class InputMessageHandler;

    /**
     * The input of one fixed simulation tick, merged over every device. Presses and
     * releases are counts, so a press and a release inside one tick give
     * PressCount 1, ReleaseCount 1, and IsHeld false: no press is lost.
     */
    struct CORE_API TickInput
    {
        /** One entry for each key that has state (held, an edge, or an axis value). */
        std::vector<std::pair<InputKey, InputKeyState>> Keys;

        /** Mouse movement this tick, in integer counts. */
        int32 MouseDeltaX = 0;
        int32 MouseDeltaY = 0;

        /** The merged state of one key: held on any device, edge counts summed, the largest axis value. */
        InputKeyState GetKeyState(InputKey t_Key) const;

        bool IsHeld(InputKey t_Key) const { return GetKeyState(t_Key).IsHeld; }

        bool WasPressed(InputKey t_Key) const { return GetKeyState(t_Key).WasPressed(); }

        bool WasReleased(InputKey t_Key) const { return GetKeyState(t_Key).WasReleased(); }

        float GetAxisValue(InputKey t_Key) const { return GetKeyState(t_Key).Value; }

        void Clear();
    };

    /**
     * Builds one TickInput for each fixed tick that runs. It has its own processor
     * (cursor) on the input queue, separate from the per-frame game processor in Input.
     *
     * Each Build takes every message that is not yet consumed. So a frame that runs no
     * tick keeps its messages for the next tick that runs, and the second tick of a
     * frame gets no edges again. This is the "first tick not yet built" rule of
     * Plans/improve-input.md. Assigning messages to ticks by their time comes with the
     * timeline of Plans/improve-time.md.
     */
    class CORE_API TickInputBuilder
    {
    public:
        explicit TickInputBuilder(std::shared_ptr<const InputMessageHandler> t_Handler);

        /**
         * Consume the new messages, write the merged state into t_Out, then clear the edges.
         * A captured keyboard or mouse (UI owns it) gives neutral input for its keys.
         */
        void Build(TickInput& t_Out, bool t_KeyboardCaptured = false, bool t_MouseCaptured = false);

    private:
        std::weak_ptr<const InputMessageHandler> m_Handler;
        InputMessageProcessor m_Processor;
    };
} // namespace Fling
