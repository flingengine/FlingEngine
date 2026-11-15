#pragma once

#include "Ecs/Access.h"
#include "Ecs/RunnerWritten.h"
#include "Ecs/SimTime.h"
#include "Ecs/System.h"
#include "Ecs/SystemContext.h"

#include <type_traits>

namespace Fling
{
    namespace Detail
    {
        template <typename T>
        concept HasFixedUpdate = requires(const T& system, typename T::Context& ctx)
        {
            system.FixedUpdate(ctx);
        };

        template <typename T>
        concept HasUpdate = requires(const T& system, typename T::Context& ctx)
        {
            system.Update(ctx);
        };

        template <typename T>
        concept HasOnCreate = requires(const T& system, typename T::Setup& setup)
        {
            system.OnCreate(setup);
        };

        template <typename T>
        concept HasOnDestroy = requires(const T& system, typename T::Setup& setup)
        {
            system.OnDestroy(setup);
        };

        template <typename T>
        concept HasOnPlayBegin = requires(const T& system, typename T::Context& ctx)
        {
            system.OnPlayBegin(ctx);
        };

        template <typename T>
        concept HasOnPlayEnd = requires(const T& system, typename T::Context& ctx)
        {
            system.OnPlayEnd(ctx);
        };

        /** The description of one system class. It is made at compile time. */
        template <typename Derived, typename A>
        inline constexpr SystemDesc DescOf{
            .TypeName = TypeNameOf<Derived>(),
            .Info = Derived::Info,
            .Access = A::Table,
            .Spawns = A::HasSpawns,
            .RawStorage = A::HasRawStorage,
            .Prepare = &PrepareSystem<A>,
        };
    } // namespace Detail

    /**
     * The base class for every system. Pass your own class and its access list:
     *
     *     class MoverSystem final : public Fling::System<MoverSystem, Fling::Access<
     *         Fling::Read<Fling::SimTime>, Fling::Write<Mover>>>
     *     {
     *     public:
     *         static constexpr Fling::SystemInfo Info { .Name = "Sandbox.Mover", .In = Fling::Order::Simulation };
     *         void FixedUpdate(Context& ctx) const;
     *     };
     *
     * Write FixedUpdate for a fixed group or Update for a variable rate group, and
     * optionally OnCreate(Setup&), OnDestroy(Setup&), OnPlayBegin(Context&), and
     * OnPlayEnd(Context&). They are not virtual: the adapter implements the ISystem
     * functions and calls yours with a context typed by the access list.
     *
     * A singleton in the list with Write, and a queue with Consume, belongs to the
     * system: the engine makes it before OnCreate and removes it after OnDestroy. So
     * most systems need no OnCreate. Write one only to give a singleton other values
     * than its defaults, or to make one that has no default constructor.
     */
    template <typename Derived, typename AccessList>
    class System : public ISystem
    {
    public:
        using Access = AccessList;
        using Context = SystemContext<Derived, AccessList>;
        using Setup = SystemSetup<AccessList>;

        const SystemDesc& Describe() const final { return Detail::DescOf<Derived, AccessList>; }

        void RunCreate(SystemSetupBase& core) const final
        {
            if constexpr (Detail::HasOnCreate<Derived>)
            {
                Setup setup(core);
                Self().OnCreate(setup);
            }
        }

        void RunDestroy(SystemSetupBase& core) const final
        {
            if constexpr (Detail::HasOnDestroy<Derived>)
            {
                Setup setup(core);
                Self().OnDestroy(setup);
            }
        }

        void RunPlayBegin(SystemContextBase& core) const final
        {
            if constexpr (Detail::HasOnPlayBegin<Derived>)
            {
                Context ctx(core);
                Self().OnPlayBegin(ctx);
            }
        }

        void RunPlayEnd(SystemContextBase& core) const final
        {
            if constexpr (Detail::HasOnPlayEnd<Derived>)
            {
                Context ctx(core);
                Self().OnPlayEnd(ctx);
            }
        }

        void RunFixedUpdate(SystemContextBase& core) const final
        {
            if constexpr (Detail::HasFixedUpdate<Derived>)
            {
                Context ctx(core);
                Self().FixedUpdate(ctx);
            }
        }

        void RunUpdate(SystemContextBase& core) const final
        {
            if constexpr (Detail::HasUpdate<Derived>)
            {
                Context ctx(core);
                Self().Update(ctx);
            }
        }

    private:
        const Derived& Self() const { return static_cast<const Derived&>(*this); }
    };

    namespace Detail
    {
        /** True if T declares an access list and an Info. */
        template <typename T>
        concept DeclaresSystem = requires
        {
            typename T::Access;
            T::Info;
        };

        /** True if T derives from the System<T, Access> adapter and declares its Info. */
        template <typename T>
        concept AdaptedSystem = DeclaresSystem<T> && std::is_base_of_v<System<T, typename T::Access>, T>;

        /** Every rule that needs the complete system class. SystemRegistry::Add calls it. */
        template <typename T>
        consteval bool CheckSystem()
        {
            static_assert(AdaptedSystem<T>,
                          "A system derives from Fling::System<Self, Fling::Access<...>> and "
                          "declares 'static constexpr Fling::SystemInfo Info'.");
            if constexpr (AdaptedSystem<T>)
            {
                using A = typename T::Access;
                constexpr SystemInfo info = T::Info;
                constexpr bool rooted = info.In.IsSet() && Detail::IsRooted(info.In);
                // Neither is true for a system with no rooted group, so only the group's assert fires.
                constexpr bool fixed = rooted && RateOf(info.In) == UpdateRate::Fixed;
                constexpr bool variable = rooted && !fixed;

                static_assert(sizeof(T) == sizeof(ISystem),
                              "A system cannot have member state. Put the state in a singleton component.");
                static_assert(std::is_default_constructible_v<T>, "A system must be default constructible.");
                static_assert(info.Name != nullptr, "A system needs a Name in its SystemInfo.");
                static_assert(info.In.IsSet(),
                              "A system needs a group in its SystemInfo, for example .In = Fling::Order::Simulation.");
                static_assert(!info.In.IsSet() || rooted,
                              "The group of a system must be inside Fling::Order::Tick or Fling::Order::Frame. "
                              "Every group except those two sets .In to its parent group.");
                static_assert(!rooted || Detail::IsResimPlacedRight(info.In),
                              "A group sets .Resim but is not directly in Fling::Order::Tick. Only a group directly in "
                              "Fling::Order::Tick can set .Resim. A group inside a resimulated group (Simulation, "
                              "Physics, ResolveDeferred) is resimulated already, so remove .Resim there.");

                static_assert(!fixed || HasFixedUpdate<T>,
                              "A fixed rate system implements void FixedUpdate(Context&) const.");
                static_assert(!fixed || !HasUpdate<T>,
                              "A fixed rate system does not implement Update. A feature that needs both is two "
                              "systems that share a component.");
                static_assert(!variable || HasUpdate<T>,
                              "A variable rate system implements void Update(Context&) const.");
                static_assert(!variable || !HasFixedUpdate<T>,
                              "A variable rate system does not implement FixedUpdate. A feature that needs both is two "
                              "systems that share a component.");

                static_assert(!WritesRunnerWritten<A>,
                              "No system writes what the runner writes (Fling::RunnerWritten): the time and the input "
                              "of each update rate. The runner writes them before the systems of their rate run. Read "
                              "them; put derived input in a component of your own.");
                static_assert(!fixed || !Allows<A, AccessMode::Read, RealFrameTime>,
                              "A fixed rate system cannot read RealFrameTime. Use SimTime.");
                static_assert(!fixed || !AnyRow(A::Table, TouchesPresentation),
                              "A fixed rate system cannot read or write a Presentation component. Drive visuals "
                              "from the simulation; do not depend on them.");
                static_assert(!fixed || !A::HasBlend,
                              "Only a frame system (under Fling::Order::Frame) declares ReadBlend<T>. A fixed rate "
                              "system runs on the tick itself: the latest value is the one it needs.");
                static_assert(!variable || !AnyRow(A::Table, WritesSimulation),
                              "A variable rate system cannot write a Simulation component. Mark the component "
                              "Presentation, or move the system to a fixed rate group.");
                static_assert(!variable || !A::HasSpawns, "A variable rate system cannot spawn entities.");
                static_assert(!variable || !A::HasRawStorage, "A variable rate system cannot use RawStorage.");
            }
            return true;
        }
    } // namespace Detail
} // namespace Fling
