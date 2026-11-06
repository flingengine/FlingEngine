#pragma once

#include "Ecs/Component.h"

/**
 * Singleton component: the Sandbox toggles for the lighting test level. T turns
 * the Rotator spin on and off, and Space turns the Mover lights on and off.
 *
 * The Sandbox.Toggles system owns it: it flips the toggles from InputState, and
 * resets them when play begins.
 */
struct SandboxToggles
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "SandboxToggles", .Kind = Fling::ComponentKind::Singleton, .Transient = true};

    bool Rotate = false;
    bool MoveLights = false;
};
