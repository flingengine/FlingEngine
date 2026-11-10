#pragma once

#include "FlingTypes.h"

#include <array>
#include <cstddef>
#include <initializer_list>

namespace Fling
{
    /** How often a phase runs. This split is the one part of the schedule that the engine owns. */
    enum class UpdateRate : uint8
    {
        /** Once per fixed simulation tick. Its systems implement FixedUpdate. */
        Fixed,
        /**
         * Once per rendered frame, so the time between two runs is not constant. For what
         * the player sees and hears. Its systems implement Update.
         */
        Variable,
    };

    inline constexpr std::size_t UpdateRateCount = 2;

    constexpr const char* UpdateRateName(const UpdateRate rate)
    {
        return rate == UpdateRate::Fixed ? "Fixed" : "Variable";
    }

    /** A short list of system or phase names, for After and Before. Write one name or a braced list. */
    struct NameList
    {
        static constexpr std::size_t Capacity = 4;

        std::array<const char*, Capacity> Names{};

        constexpr NameList() = default;

        constexpr NameList(const char* name) : Names{name} {}

        constexpr NameList(std::initializer_list<const char*> names)
        {
            if (names.size() > Capacity)
            {
                TooManyNames();
            }
            std::size_t next = 0;
            for (const char* name : names)
            {
                if (next < Capacity)
                {
                    Names[next++] = name;
                }
            }
        }

        constexpr std::size_t Size() const
        {
            std::size_t count = 0;
            while (count < Capacity && Names[count] != nullptr)
            {
                ++count;
            }
            return count;
        }

        constexpr const char* const* begin() const { return Names.data(); }

        constexpr const char* const* end() const { return Names.data() + Size(); }

    private:
        /** Not constexpr, so a list that is too long does not compile when it is a constant. */
        static void TooManyNames() {}
    };

    /**
     * A named group of systems, placed with After and Before. Any module can
     * declare one as an `inline constexpr Fling::PhaseInfo`. The engine's own
     * phases are in Fling::Phases. Order inside an update rate comes from the
     * phases and from what each system reads and writes (Plans/ecs-systems.md 3.8).
     */
    struct PhaseInfo
    {
        /** "Module.Name". Systems and other phases use it in After and Before. */
        const char* Name = nullptr;
        Fling::UpdateRate UpdateRate = Fling::UpdateRate::Fixed;
        /** Phases of the same update rate that run before this one. */
        NameList After{};
        /** Phases of the same update rate that run after this one. */
        NameList Before{};
        /**
         * Apply the deferred commands when this phase ends. The phase must then be
         * ordered against every other phase of its update rate. The end of each
         * fixed tick always applies them, so most phases do not need this.
         */
        bool FlushAfter = false;
    };

    /** A reference to an `inline constexpr` PhaseInfo, so that a system's update rate is known at compile time. */
    class PhaseRef
    {
    public:
        constexpr PhaseRef() = default;

        constexpr PhaseRef(const PhaseInfo& phase) : m_Phase(&phase) {}

        constexpr bool IsSet() const { return m_Phase != nullptr; }

        constexpr const PhaseInfo& Get() const { return *m_Phase; }

        constexpr const PhaseInfo* operator->() const { return m_Phase; }

    private:
        const PhaseInfo* m_Phase = nullptr;
    };

    constexpr bool IsFixed(const PhaseRef phase)
    {
        return phase.IsSet() && phase->UpdateRate == UpdateRate::Fixed;
    }

    /**
     * The engine's phases. They are anchors that other modules place their own
     * phases against, with After and Before. Do not rename them: other modules use the names.
     *
     * One fixed tick is Input, Simulation, Physics, ResolveDeferred, Finalize. A
     * resimulation (rollback) runs Simulation, Physics, and ResolveDeferred again. It
     * does not run Input, which is replaced by recorded input, or Finalize, which runs
     * one time when the simulation has its last state. One frame is Presentation,
     * Publish, Editor, after the ticks that were due.
     */
    // clang-format off: clang-format puts the { of a designated initializer on the line of the name.
    namespace Phases
    {
        /** Turns this tick's input into InputState. */
        inline constexpr PhaseInfo Input
        {
            .Name = "Engine.Input",
            .UpdateRate = UpdateRate::Fixed,
        };

        /** Gameplay. */
        inline constexpr PhaseInfo Simulation
        {
            .Name = "Engine.Simulation",
            .UpdateRate = UpdateRate::Fixed,
            .After = "Engine.Input",
        };

        /**
         * The physics step: moves bodies, and writes the contacts of this tick. After
         * gameplay, which asks for forces, and before ResolveDeferred, so that its
         * consumers see the contacts of this tick.
         */
        inline constexpr PhaseInfo Physics
        {
            .Name = "Engine.Physics",
            .UpdateRate = UpdateRate::Fixed,
            .After = "Engine.Simulation",
        };

        /** Consumes the queues that Simulation appends to: spawns, damage, effects. */
        inline constexpr PhaseInfo ResolveDeferred
        {
            .Name = "Engine.ResolveDeferred",
            .UpdateRate = UpdateRate::Fixed,
            .After = "Engine.Physics",
        };

        /**
         * The simulation has its last state for this tick. Systems here make the data that
         * comes from that state, such as LocalToWorld from Transform and Parent. They do
         * not change what the simulation decided.
         */
        inline constexpr PhaseInfo Finalize
        {
            .Name = "Engine.Finalize",
            .UpdateRate = UpdateRate::Fixed,
            .After = "Engine.ResolveDeferred",
        };

        /**
         * Frame rate work for what the player sees and hears: camera smoothing,
         * interpolation, UI. Its systems write components of ComponentDomain::Presentation.
         */
        inline constexpr PhaseInfo Presentation
        {
            .Name = "Engine.Presentation",
            .UpdateRate = UpdateRate::Variable,
        };

        /** Gives the renderer its copy of the world (RenderSnapshot). The renderer reads nothing else. */
        inline constexpr PhaseInfo Publish
        {
            .Name = "Engine.Publish",
            .UpdateRate = UpdateRate::Variable,
            .After = "Engine.Presentation",
        };

        /** Editor tools, last in the frame. */
        inline constexpr PhaseInfo Editor
        {
            .Name = "Engine.Editor",
            .UpdateRate = UpdateRate::Variable,
            .After = "Engine.Publish",
        };

        /** Every engine phase, so that a schedule always has the anchors, even with no systems in them. */
        inline constexpr std::array<const PhaseInfo*, 8> All
        {
            &Input,
            &Simulation,
            &Physics,
            &ResolveDeferred,
            &Finalize,
            &Presentation,
            &Publish,
            &Editor,
        };
    } // namespace Phases
    // clang-format on

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
