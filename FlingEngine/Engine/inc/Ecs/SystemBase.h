#pragma once

#include "Ecs/Access.h"
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
     *         static constexpr Fling::SystemInfo Info { .Name = "Sandbox.Mover", .Phase = Fling::Phases::Simulation };
     *         void FixedUpdate(Context& ctx) const;
     *     };
     *
     * Write FixedUpdate for a fixed phase or Update for a variable rate phase, and
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
                constexpr bool fixed = IsFixed(info.Phase);

                static_assert(sizeof(T) == sizeof(ISystem),
                              "A system cannot have member state. Put the state in a singleton component.");
                static_assert(std::is_default_constructible_v<T>, "A system must be default constructible.");
                static_assert(info.Name != nullptr, "A system needs a Name in its SystemInfo.");
                static_assert(
                    info.Phase.IsSet(),
                    "A system needs a Phase in its SystemInfo, for example .Phase = Fling::Phases::Simulation.");

                static_assert(!fixed || HasFixedUpdate<T>, "A fixed phase system implements FixedUpdate(Context&).");
                static_assert(!fixed || !HasUpdate<T>,
                              "A fixed phase system does not implement Update. A feature that needs both is two "
                              "systems that share a component.");
                static_assert(fixed || HasUpdate<T>, "A variable rate system implements Update(Context&).");
                static_assert(fixed || !HasFixedUpdate<T>,
                              "A variable rate system does not implement FixedUpdate. A feature that needs both is two "
                              "systems that share a component.");

                static_assert(!Allows<A, AccessMode::Write, SimTime> && !Allows<A, AccessMode::Write, RealFrameTime>,
                              "No system writes SimTime or RealFrameTime. The runner writes them.");
                static_assert(!fixed || !Allows<A, AccessMode::Read, RealFrameTime>,
                              "A fixed rate system cannot read RealFrameTime. Use SimTime.");
                static_assert(!fixed || !ReadsPresentation<A>,
                              "A fixed rate system cannot read a Presentation component. Drive visuals from the "
                              "simulation; do not depend on them.");
                static_assert(fixed || !WritesSimulation<A>,
                              "A variable rate system cannot write a Simulation component. Mark the component "
                              "Presentation, or move the system to a fixed phase.");
                static_assert(fixed || !A::HasSpawns, "A variable rate system cannot spawn entities.");
                static_assert(fixed || !A::HasRawStorage, "A variable rate system cannot use RawStorage.");
            }
            return true;
        }
    } // namespace Detail
} // namespace Fling
