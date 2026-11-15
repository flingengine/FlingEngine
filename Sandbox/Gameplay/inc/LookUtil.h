#pragma once

#include "FlingTypes.h"
#include "Input/InputSlice.h"
#include "LookCamera.h"

#include <algorithm>
#include <cstddef>
#include <span>

/**
 * The pure rule of the look camera. The fixed tick and the frame side call the same
 * function (Run), so the view on screen is what the simulation will make from the same
 * input. No state, no globals.
 */
namespace LookUtil
{
    /** The steps of Run. Tests call them; game code calls Run. */
    namespace Detail
    {
        /**
         * Turn the view by mouse counts, then clamp the pitch. A policy that is frozen (F) or
         * in stasis (gameplay) gives the view back unchanged.
         */
        inline LookView Apply(LookView t_View, const int32 t_CountsX, const int32 t_CountsY, const LookPolicy& t_Policy)
        {
            if (t_Policy.Frozen || t_Policy.InStasis)
            {
                return t_View;
            }
            // Mouse right turns right (yaw goes down) and mouse down looks down, as FirstPersonCamera does
            t_View.Yaw -= static_cast<float>(t_CountsX) * t_Policy.LookScale;
            t_View.Pitch = std::clamp(t_View.Pitch - static_cast<float>(t_CountsY) * t_Policy.LookScale,
                                      t_Policy.PitchMin,
                                      t_Policy.PitchMax);
            return t_View;
        }

        /**
         * Turn the view by each mouse sample in order, clamping after each one. So the result
         * does not depend on how the samples are split into calls:
         * Look(Look(v, a), b) == Look(v, a + b).
         */
        inline LookView
        Look(LookView t_View, const std::span<const Fling::InputMouseSample> t_Samples, const LookPolicy& t_Policy)
        {
            for (const Fling::InputMouseSample& Sample : t_Samples)
            {
                t_View = Apply(t_View, Sample.DeltaX, Sample.DeltaY, t_Policy);
            }
            return t_View;
        }

        /** The game's keys: F toggles Frozen, Minus halves LookScale, Equal doubles it, each on the press. */
        inline bool ApplyKey(LookPolicy& t_Policy, const Fling::InputKeyEvent& t_Key)
        {
            if (!t_Key.Down)
            {
                return false;
            }
            if (t_Key.Key == Fling::Keys::F)
            {
                t_Policy.Frozen = !t_Policy.Frozen;
            }
            else if (t_Key.Key == Fling::Keys::Minus)
            {
                t_Policy.LookScale *= 0.5f;
            }
            else if (t_Key.Key == Fling::Keys::Equal)
            {
                t_Policy.LookScale *= 2.0f;
            }
            else
            {
                return false;
            }
            return true;
        }
    } // namespace Detail

    /**
     * The look camera's rule for one stretch of input: walk the mouse samples and the key
     * edges of the slice in stream order. A key press changes the policy at its own place,
     * so the samples before it turn the view under the old policy and the samples after it
     * under the new one. Sandbox.LookCamera runs it on each tick's input
     * (TickInput::Slice); Sandbox.LookCameraView runs it on the pending input
     * (PendingInput), from the camera of the last tick. Run(Run(c, a), b) == Run(c, a + b),
     * so the frame shows exactly what the ticks will make, however the input is split.
     */
    inline LookCamera Run(LookCamera t_Camera, const Fling::InputSlice& t_Input)
    {
        const std::span<const Fling::InputMouseSample> Samples = t_Input.MouseSamples;
        std::size_t RunStart = 0;
        std::size_t Next = 0;
        Fling::ForEachInOrder(
            Samples,
            t_Input.KeyEvents,
            [&](const Fling::InputMouseSample&) { ++Next; },
            [&](const Fling::InputKeyEvent& t_Key)
            {
                LookPolicy Changed = t_Camera.Policy;
                if (Detail::ApplyKey(Changed, t_Key))
                {
                    t_Camera.View =
                        Detail::Look(t_Camera.View, Samples.subspan(RunStart, Next - RunStart), t_Camera.Policy);
                    RunStart = Next;
                    t_Camera.Policy = Changed;
                }
            });
        t_Camera.View = Detail::Look(t_Camera.View, Samples.subspan(RunStart), t_Camera.Policy);
        return t_Camera;
    }
} // namespace LookUtil
