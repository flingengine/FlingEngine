#pragma once

#include "Ecs/Component.h"

/**
 * Singleton component: the Sandbox toggles for the lighting test level. T turns
 * the Rotator spin on and off, and Space turns the Mover lights on and off.
 *
 * The Sandbox.Toggles system owns it: it flips the toggles from InputSnapshot, and
 * resets them when play begins.
 */
struct SandboxToggles
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "SandboxToggles", .Kind = Fling::ComponentKind::Singleton, .Transient = true};

    bool Rotate = false;
    bool MoveLights = false;
};

/** Singleton component: where the next test sphere goes. The Sandbox.TestSpawn system owns it. */
struct SandboxSpawnState
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "SandboxSpawnState", .Kind = Fling::ComponentKind::Singleton, .Transient = true};

    /** The X position of the last sphere. Each spawn goes 1 unit further left. */
    float LastX = -1.0f;
};
