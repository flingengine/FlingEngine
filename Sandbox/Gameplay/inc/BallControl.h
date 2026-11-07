#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

/**
 * Lets the player push a physics body. The arrow keys (or the gamepad D-pad)
 * push it on the XZ plane, and Space (or the A button) makes it jump. The entity
 * also needs a RigidBody and a Collider.
 */
struct BallControl
{
    static constexpr Fling::ComponentInfo Component{.Name = "BallControl"};

    /** The force of a push, in newtons. */
    float m_PushForce = 40.0f;

    /** The upward impulse of a jump, in newton seconds. */
    float m_JumpImpulse = 25.0f;
};

/** Singleton component: Sandbox state for the physics test level. The Sandbox.FirstHit system owns it. */
struct SandboxPhysicsState
{
    static constexpr Fling::ComponentInfo Component{
        .Name = "SandboxPhysicsState", .Kind = Fling::ComponentKind::Singleton, .Transient = true};

    /** True after the first contact between a BallControl body and another moving body. */
    bool m_HasLoggedHit = false;

    /** The number of game ticks since the game started. */
    uint64 m_Tick = 0;
};
