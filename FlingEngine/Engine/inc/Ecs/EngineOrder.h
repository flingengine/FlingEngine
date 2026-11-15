#pragma once

#include "Ecs/Order.h"

#include <array>

namespace Fling
{
    /**
     * The engine's groups. Other modules put their own groups and systems in them, and
     * order against them, with In, After, and Before. Do not rename them: the name is
     * the identity of a group. Order against a group where one fits: a system can split
     * later, a group stays.
     *
     * One fixed tick (Tick) is Input, Simulation, Physics, ResolveDeferred, Finalize. A
     * resimulation (rollback) runs Simulation, Physics, and ResolveDeferred again: they set
     * GroupInfo::Resim, so every group and system in them is in the resimulation, and
     * Schedule::ResimRunOrder lists what it runs. It does not run Input, which is replaced
     * by recorded input, or Finalize, which runs one time when the simulation has its last
     * state: Finalize makes only Rebuild data, and no resimulated system may read it
     * (ScheduleErrorKind::ResimSkipsState). One frame (Frame) is Presentation, Render,
     * Editor, after the ticks that were due.
     */
    namespace Order
    {
        /** The root of everything that runs once per fixed tick: the simulation (the game thread in threaded play). */
        inline constexpr GroupInfo Tick{.Name = TickRootName};

        /** The root of everything that runs once per rendered frame: the frame side, on the main thread. */
        inline constexpr GroupInfo Frame{.Name = FrameRootName};

        /** Turns this tick's input into InputState. */
        inline constexpr GroupInfo Input{.Name = "Engine.Input", .In = Tick};

        /** Gameplay. */
        inline constexpr GroupInfo Simulation{.Name = "Engine.Simulation", .In = Tick, .After = Input, .Resim = true};

        /**
         * The physics step: moves bodies, and writes the contacts of this tick. After
         * gameplay, which asks for forces, and before ResolveDeferred, so that its
         * consumers see the contacts of this tick.
         */
        inline constexpr GroupInfo Physics{.Name = "Engine.Physics", .In = Tick, .After = Simulation, .Resim = true};

        /** Consumes the queues that Simulation appends to: spawns, damage, effects. */
        inline constexpr GroupInfo ResolveDeferred{
            .Name = "Engine.ResolveDeferred", .In = Tick, .After = Physics, .Resim = true};

        /**
         * The simulation has its last state for this tick. Systems here make the data that
         * comes from that state, such as LocalToWorld from Transform and Parent. They do
         * not change what the simulation decided.
         */
        inline constexpr GroupInfo Finalize{.Name = "Engine.Finalize", .In = Tick, .After = ResolveDeferred};

        /**
         * Frame rate work for what the player sees and hears: camera smoothing,
         * interpolation, UI. Its systems write components of ComponentDomain::Presentation.
         */
        inline constexpr GroupInfo Presentation{.Name = "Engine.Presentation", .In = Frame};

        /** Gives the renderer its copy of the world (RenderSnapshot), after the Presentation systems. */
        inline constexpr GroupInfo Render{.Name = "Engine.Render", .In = Frame, .After = Presentation};

        /** Editor tools, last in the frame. */
        inline constexpr GroupInfo Editor{.Name = "Engine.Editor", .In = Frame, .After = Render};

        /**
         * Every engine group. The schedule builder always adds them, so that a schedule
         * lists them even with no systems in them. A child points to its parent, not
         * the reverse, so the builder cannot find them from Tick and Frame.
         */
        inline constexpr auto All = std::to_array<const GroupInfo*>({
            &Tick,
            &Frame,
            &Input,
            &Simulation,
            &Physics,
            &ResolveDeferred,
            &Finalize,
            &Presentation,
            &Render,
            &Editor,
        });
    } // namespace Order
} // namespace Fling
