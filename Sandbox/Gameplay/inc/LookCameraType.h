#pragma once

#include "Ecs/ModuleRegistration.h"
#include "LookCamera.h"

namespace Sandbox
{
    /**
     * Registers the LookCamera component with its fields. The Sandbox registers it with
     * this, and so do the tests that run the look camera systems, so the state hash reads
     * the same fields everywhere.
     */
    inline void RegisterLookCameraType(Fling::ModuleRegistration& t_Modules)
    {
        t_Modules.Component<LookCamera>(
            [](Fling::TypeBuilder<LookCamera>& t_Type)
            {
                t_Type.Category("Camera");
                t_Type.Field("yaw", &LookCamera::View, &LookView::Yaw).Tooltip("Radians");
                t_Type.Field("pitch", &LookCamera::View, &LookView::Pitch).Tooltip("Radians");
                t_Type.Field("lookScale", &LookCamera::Policy, &LookPolicy::LookScale)
                    .Tooltip("Radians per mouse count");
                t_Type.Field("frozen", &LookCamera::Policy, &LookPolicy::Frozen);
                // Gameplay state that Stasis drives: not saved to a level, still in the state hash
                t_Type.Field("inStasis", &LookCamera::Policy, &LookPolicy::InStasis).Transient();
                t_Type.Field("pitchMin", &LookCamera::Policy, &LookPolicy::PitchMin);
                t_Type.Field("pitchMax", &LookCamera::Policy, &LookPolicy::PitchMax);
            });
    }
} // namespace Sandbox
