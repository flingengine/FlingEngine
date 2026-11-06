#pragma once

#include "Ecs/Access.h"
#include "Ecs/Phase.h"

#include <span>

namespace Fling
{
    class SystemContextBase;
    class SystemSetupBase;

    /** The declaration on a system class: `static constexpr Fling::SystemInfo Info { .Name = "...", .Phase = ... };`.
     */
    struct SystemInfo
    {
        /** "Module.Name". Errors, the schedule dump, After, Before, and profiling use it. */
        const char* Name = nullptr;
        /** The phase, for example Fling::Phases::Simulation. Its update rate decides FixedUpdate or Update. */
        PhaseRef Phase{};
        /** The world roles that run this system. */
        WorldRole RunIn = WorldRole::Play;
        /**
         * Systems of the same update rate that must run before this one. Replaces the
         * inferred edge between the two. A name in a phase that phase order puts
         * after this one is a cycle.
         */
        NameList After{};
        /** Systems of the same update rate that must run after this one. Replaces the inferred edge between the two. */
        NameList Before{};
        /** For a system that touches GLFW or ImGui. A parallel executor runs it on the main thread. */
        bool MainThreadOnly = false;
        /** Reserved for rollback: run FixedUpdate again during a resimulation. */
        bool ReplayOnResim = false;
    };

    /** What the schedule builder knows about one system. It is made at compile time from the class. */
    struct SystemDesc
    {
        SystemInfo Info{};
        /** One row per component type in the system's Access<...> list. */
        std::span<const AccessEntry> Access{};
        /** True if the access list has Spawns. */
        bool Spawns = false;
        /** True if the access list has RawStorage. A parallel executor runs such a system alone. */
        bool RawStorage = false;
        /** Makes the storage for every component in the access table, so a query never changes the storage. */
        void (*PrepareStorage)(SystemSetupBase& setup) = nullptr;
    };

    /**
     * A system: functions and no state. The world owns one instance of each
     * system, and the instance has no members, so all state goes in singleton
     * components that the system makes in OnCreate.
     *
     * Do not derive from ISystem directly. Derive from the Fling::System<Self,
     * Access<...>> adapter (Ecs/SystemBase.h). The adapter implements these Run*
     * functions, and calls the OnCreate, FixedUpdate, or Update that you write
     * with a context typed by your access list. Only the runner calls Run*.
     */
    class ISystem
    {
    public:
        virtual ~ISystem() = default;

        /** The name, phase, roles, and access table. Made at compile time. */
        virtual const SystemDesc& Describe() const = 0;

        /** Called once when the world is made, in schedule order, before the first level loads. */
        virtual void RunCreate(SystemSetupBase& setup) const = 0;

        /** Called once when the world shuts down, in reverse schedule order. */
        virtual void RunDestroy(SystemSetupBase& setup) const = 0;

        /** Called when the world starts to play, after the edit snapshot. It counts as setup for tick 0. */
        virtual void RunPlayBegin(SystemContextBase& ctx) const = 0;

        /** Called when play stops, before the edit snapshot is restored. */
        virtual void RunPlayEnd(SystemContextBase& ctx) const = 0;

        /** Called once per simulation tick, for a system in a fixed phase. */
        virtual void RunFixedUpdate(SystemContextBase& ctx) const = 0;

        /** Called once per rendered frame, for a system in a frame phase. */
        virtual void RunUpdate(SystemContextBase& ctx) const = 0;
    };
} // namespace Fling
