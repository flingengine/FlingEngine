#pragma once

#include "Ecs/Component.h"
#include "FlingMath.h"
#include "FlingTypes.h"

#include <vector>

/**
 * Presentation singleton: how evenly the meshes turned on screen, frame by frame, during
 * play. The Sandbox.MotionProbe system owns it, and logs it when play ends, so a headless
 * run shows whether the frames between ticks are interpolated. Both are registered only
 * under --null-render: test instrumentation, not part of the game (Docs/threading-model.md,
 * "The two blend states"). The Rotator spheres turn at a constant rate, so every frame that
 * turns them should turn them by the same angle, and no frame should repeat the one before.
 */
struct MotionProbe
{
    static constexpr Fling::ComponentInfo Component{.Name = "MotionProbe",
                                                    .Kind = Fling::ComponentKind::Singleton,
                                                    .Domain = Fling::ComponentDomain::Presentation,
                                                    .Transient = true};

    /** The last frame's meshes: their keys and their X axes (world matrix column 0). */
    std::vector<uint64> Keys;
    std::vector<glm::vec3> Axes;

    /**
     * Frames in which a mesh turned, and the smallest and largest turn of one frame, in
     * degrees. The first and the last frame of each run of turning frames are left out: the
     * turn starts and stops at a tick, which a frame can show part of the way through.
     */
    uint32 TurnFrames = 0;
    double MinTurn = 0.0;
    double MaxTurn = 0.0;

    /** Frames with no turn between two frames that turned: a frame that showed the one before again. */
    uint32 Repeated = 0;

    /** Frames with no turn since the last one that turned. */
    uint32 Still = 0;

    /** The turning frames in the current run, and the turn of the newest one (counted once the next one turns). */
    uint32 RunFrames = 0;
    double LastTurn = 0.0;

    /** True once a frame turned. */
    bool Turned = false;

    /** Add one frame's largest turn of a mesh, in degrees. */
    void Add(const double t_Degrees)
    {
        if (t_Degrees <= 1e-6)
        {
            Still += Turned ? 1 : 0;
            RunFrames = 0;
            return;
        }
        Repeated += Still;
        Still = 0;
        Turned = true;
        if (RunFrames >= 2)
        {
            MinTurn = TurnFrames == 0 ? LastTurn : Fling::Math::Min(MinTurn, LastTurn);
            MaxTurn = TurnFrames == 0 ? LastTurn : Fling::Math::Max(MaxTurn, LastTurn);
            ++TurnFrames;
        }
        LastTurn = t_Degrees;
        ++RunFrames;
    }

    /** True if every frame that turned turned by the same angle, to 1 percent. */
    bool Even() const { return TurnFrames > 0 && MaxTurn - MinTurn <= 0.01 * MaxTurn; }
};
