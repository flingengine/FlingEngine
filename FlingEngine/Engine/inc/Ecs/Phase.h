#pragma once

#include "FlingTypes.h"

#include <cstddef>

namespace Fling
{
    /**
     * The point in the frame where a system runs. The phase also decides the
     * update rate: Input, Simulation, and ResolveDeferred run at the fixed tick.
     * PostSimulation, Extract, and Editor run once per rendered frame.
     */
    enum class Phase : uint8
    {
        Input,
        Simulation,
        ResolveDeferred,
        PostSimulation,
        Extract,
        Editor,
    };

    inline constexpr std::size_t PhaseCount = 6;

    /** How often a phase runs. */
    enum class UpdateRate : uint8
    {
        /** Once per fixed simulation tick. The system implements FixedUpdate. */
        Fixed,
        /** Once per rendered frame. The system implements Update. */
        Frame,
    };

    constexpr UpdateRate UpdateRateOf(const Phase phase)
    {
        return phase <= Phase::ResolveDeferred ? UpdateRate::Fixed : UpdateRate::Frame;
    }

    constexpr bool IsFixedPhase(const Phase phase)
    {
        return UpdateRateOf(phase) == UpdateRate::Fixed;
    }

    constexpr const char* PhaseName(const Phase phase)
    {
        switch (phase)
        {
        case Phase::Input:
            return "Input";
        case Phase::Simulation:
            return "Simulation";
        case Phase::ResolveDeferred:
            return "ResolveDeferred";
        case Phase::PostSimulation:
            return "PostSimulation";
        case Phase::Extract:
            return "Extract";
        case Phase::Editor:
            return "Editor";
        }
        return "Unknown";
    }

    /** The job of a world. A bit mask: a system's RunIn says which roles it runs in. */
    enum class WorldRole : uint8
    {
        None = 0,
        /** The world the editor changes. */
        Edit = 1 << 0,
        /** A world that runs the game. */
        Play = 1 << 1,
        /** A small world for an asset or prefab preview. */
        Preview = 1 << 2,
        All = Edit | Play | Preview,
    };

    constexpr WorldRole operator|(const WorldRole a, const WorldRole b)
    {
        return static_cast<WorldRole>(static_cast<uint8>(a) | static_cast<uint8>(b));
    }

    constexpr WorldRole operator&(const WorldRole a, const WorldRole b)
    {
        return static_cast<WorldRole>(static_cast<uint8>(a) & static_cast<uint8>(b));
    }

    /** True if the two masks share a role. */
    constexpr bool Overlaps(const WorldRole a, const WorldRole b)
    {
        return (a & b) != WorldRole::None;
    }
} // namespace Fling
