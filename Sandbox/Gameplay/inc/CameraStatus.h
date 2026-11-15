#pragma once

#include "Ecs/Component.h"
#include "FlingTypes.h"

/** What the look camera does now, as the frame side shows it. */
enum class CameraMode : uint8
{
    /** No look camera (the editor before Play). */
    None,
    Free,
    /** The player froze it (F). */
    Frozen,
    /** Gameplay stopped it (Stasis). */
    Stasis,
};

/**
 * Presentation singleton: the look camera's state for the UI, made each frame from the
 * published LookCamera and Stasis (Sandbox.CameraStatus). The UI reads it from the
 * storage the frame side reads (World::GetPresentedStorage), never from the simulation.
 */
struct CameraStatus
{
    static constexpr Fling::ComponentInfo Component{.Name = "CameraStatus",
                                                    .Kind = Fling::ComponentKind::Singleton,
                                                    .Domain = Fling::ComponentDomain::Presentation,
                                                    .Transient = true};

    CameraMode Mode = CameraMode::None;

    /** Simulation time of stasis left, in seconds, when Mode is Stasis. */
    float StasisSecondsLeft = 0.0f;
};
