#pragma once

#include "Ecs/Component.h"

/**
 * Singleton component: the Sandbox toggles for the lighting test level. T turns
 * the Rotator spin on and off, and Space turns the Mover lights on and off.
 *
 * Game::Update still reads the input and flips these, so the key handling is
 * the same as before the systems. That moves into an Input phase system when
 * per-tick input lands (Plans/improve-input.md).
 */
struct SandboxToggles
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "SandboxToggles", .Kind = Fling::ComponentKind::Singleton, .Transient = true};

    bool Rotate = false;
    bool MoveLights = false;
};
