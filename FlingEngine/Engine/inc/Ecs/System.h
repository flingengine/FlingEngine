#pragma once

#include "Ecs/Access.h"
#include "Ecs/EngineOrder.h"
#include "Ecs/Order.h"
#include "Ecs/TypeHash.h"

#include <span>
#include <vector>

namespace Fling
{
    class EntityStorage;
    class SystemContextBase;
    class SystemSetupBase;

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

    /** The declaration on a system class: `static constexpr Fling::SystemInfo Info { .Name = "...", .In = ... };`. */
    struct SystemInfo
    {
        /** "Module.Name". Errors, the schedule dump, the tie-break, and profiling use it. */
        const char* Name = nullptr;
        /** The group, for example Fling::Order::Simulation. Its update rate decides FixedUpdate or Update. */
        GroupRef In{};
        /** The world roles that run this system. */
        WorldRole RunIn = WorldRole::Play;
        /**
         * Groups and systems of the same update rate that must run before this one. An
         * order replaces the inferred edge between the two. A reference to something that
         * group order puts after this system is a cycle.
         */
        OrderList After{};
        /** Groups and systems of the same update rate that must run after this one. */
        OrderList Before{};
    };

    /** What the schedule builder knows about one system. It is made at compile time from the class. */
    struct SystemDesc
    {
        /**
         * TypeNameOf<T>() of the system class. SystemOf<T> finds the system by its id,
         * SystemTypeId<T>() (Detail::SystemTypeHash of this name), and a fix in a message
         * names the class with it. Empty for a SystemDesc made by hand: SystemOf cannot find it.
         */
        StringView TypeName{};
        SystemInfo Info{};
        /** One row per component type in the system's Access<...> list. */
        std::span<const AccessEntry> Access{};
        /** True if the access list has Spawns. */
        bool Spawns = false;
        /** True if the access list has RawStorage. A future parallel executor would run it alone. */
        bool RawStorage = false;
        /**
         * Gets the storage ready for the system. It gives every component type in the access
         * table its id, so the first tick does not. And it makes each singleton that the
         * system reads or writes and each queue that it consumes, of the given domain, if
         * the storage does not have it yet, and adds a function to made that removes it
         * (Detail::EngineMakes). The runner calls it before any OnCreate, and calls the
         * functions at shutdown.
         */
        void (*Prepare)(EntityStorage& storage,
                        ComponentDomain domain,
                        std::vector<void (*)(EntityStorage&)>& made) = nullptr;
    };

    /**
     * A system: functions and no state. The world owns one instance of each
     * system, and the instance has no members, so all state goes in singleton
     * components. The engine makes a singleton for the system that writes it.
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

        /** The name, group, roles, and access table. Made at compile time. */
        virtual const SystemDesc& Describe() const = 0;

        /** Called once when the world is made, in schedule order, before the first level loads. */
        virtual void RunCreate(SystemSetupBase& setup) const = 0;

        /** Called once when the world shuts down, in reverse schedule order. */
        virtual void RunDestroy(SystemSetupBase& setup) const = 0;

        /** Called when the world starts to play, after the edit snapshot. It counts as setup for tick 0. */
        virtual void RunPlayBegin(SystemContextBase& ctx) const = 0;

        /** Called when play stops, before the edit snapshot is restored. */
        virtual void RunPlayEnd(SystemContextBase& ctx) const = 0;

        /** Called once per simulation tick, for a system in a fixed group. */
        virtual void RunFixedUpdate(SystemContextBase& ctx) const = 0;

        /** Called once per rendered frame, for a system in a variable rate group. */
        virtual void RunUpdate(SystemContextBase& ctx) const = 0;
    };
} // namespace Fling
