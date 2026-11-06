#pragma once

#include "Ecs/Access.h"
#include "Ecs/Phase.h"

#include <array>
#include <cstddef>
#include <initializer_list>
#include <span>

namespace Fling
{
    class SystemContextBase;
    class SystemSetupBase;

    /** A short list of system names, for SystemInfo::After and SystemInfo::Before. Write one name or a braced list. */
    struct SystemNames
    {
        static constexpr std::size_t Capacity = 4;

        std::array<const char*, Capacity> Names{};

        constexpr SystemNames() = default;

        constexpr SystemNames(const char* name) : Names{name} {}

        constexpr SystemNames(std::initializer_list<const char*> names)
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

    /** The declaration on a system class: `static constexpr Fling::SystemInfo Info { .Name = "...", .Phase = ... };`.
     */
    struct SystemInfo
    {
        /** "Module.Name". Errors, the schedule dump, After, Before, and profiling use it. */
        const char* Name = nullptr;
        /** The phase decides the update rate: FixedUpdate for a fixed phase, Update for a frame phase. */
        Fling::Phase Phase = Fling::Phase::Simulation;
        /** The world roles that run this system. */
        WorldRole RunIn = WorldRole::Play;
        /** Systems in the same phase that must run before this one. Replaces the inferred edge between the two. */
        SystemNames After{};
        /** Systems in the same phase that must run after this one. Replaces the inferred edge between the two. */
        SystemNames Before{};
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
