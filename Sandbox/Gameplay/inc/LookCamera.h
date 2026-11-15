#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

#include <cstddef>

/** A look direction in radians. Yaw turns around the world up axis, Pitch looks up (positive) and down. */
struct LookView
{
    float Yaw = 0.0f;
    float Pitch = 0.0f;

    bool operator==(const LookView&) const = default;
};
// The same layout on every platform: a build where this fails has drifted from the others
static_assert(sizeof(LookView) == 8 && offsetof(LookView, Pitch) == 4);

/**
 * How mouse counts turn the view. The simulation owns it (in LookCamera), and the
 * frame side uses it as plain data, so the simulation can freeze or retune the camera
 * without the frame code knowing why.
 */
struct LookPolicy
{
    /** Radians per mouse count. */
    float LookScale = 0.0025f;

    /** The player's own freeze (the F key, at its place in the input). True: mouse input is thrown away. */
    bool Frozen = false;

    /**
     * A freeze that gameplay sets (Sandbox.Stasis), not input. True: mouse input is thrown
     * away. The frame side sees it from the first frame that shows the tick that set it.
     */
    bool InStasis = false;

    float PitchMin = -1.5f;
    float PitchMax = 1.5f;

    bool operator==(const LookPolicy&) const = default;
};
// The same layout on every platform: a build where this fails has drifted from the others
static_assert(sizeof(LookPolicy) == 16 && offsetof(LookPolicy, Frozen) == 4 && offsetof(LookPolicy, InStasis) == 5 &&
              offsetof(LookPolicy, PitchMin) == 8 && offsetof(LookPolicy, PitchMax) == 12);

/**
 * The simulation camera: the view and the policy that the fixed tick has made from the
 * input it consumed (Sandbox.LookCamera, with LookUtil::Run). Key presses change the
 * policy at their place in the input; other fixed systems can change it too
 * (Sandbox.Stasis sets InStasis). It is the only thing that the simulation and the frame
 * side (Sandbox.LookCameraView) share. Game state: a rollback restores it, and the state
 * hash covers it.
 */
struct LookCamera
{
    static constexpr Fling::ComponentInfo Component{.Name = "LookCamera", .Rollback = Fling::RollbackClass::Restore};

    LookView View;
    LookPolicy Policy;
};
// The same layout on every platform: a build where this fails has drifted from the others
static_assert(sizeof(LookCamera) == 24 && offsetof(LookCamera, Policy) == 8);
