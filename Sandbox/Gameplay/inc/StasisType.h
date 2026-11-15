#pragma once

#include "Ecs/ModuleRegistration.h"
#include "Stasis.h"

namespace Sandbox
{
    /**
     * Registers the Stasis component with its field, so the state hash covers it. The
     * Sandbox registers it with this, and so can a test that hashes the state.
     */
    inline void RegisterStasisType(Fling::ModuleRegistration& t_Modules)
    {
        t_Modules.Component<Stasis>(
            [](Fling::TypeBuilder<Stasis>& t_Type)
            {
                t_Type.Category("Camera");
                t_Type.Field("remainingNs", &Stasis::RemainingNs).Tooltip("Simulation time left, in nanoseconds");
            });
    }
} // namespace Sandbox
