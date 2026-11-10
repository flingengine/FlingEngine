#pragma once

#include "Allocators/LinearArena.h"
#include "Ecs/Access.h"
#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/Phase.h"
#include "Ecs/Query.h"
#include "Ecs/QueryFilters.h"
#include "Ecs/TypeList.h"
#include "Input/TickInput.h"
#include "MathUtils.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <functional>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fling
{
    /**
     * Changes that wait until the end of the fixed tick. Each system has its own buffer.
     *
     * At the flush, the typed adds and removes of every system are grouped by entity,
     * so each entity moves to its final archetype once, however many systems changed
     * it. Then each system's other deferred changes (Defer) run in schedule order, and
     * last the destroys.
     *
     * The values to add and the deferred callables live in the buffer's LinearArena,
     * not on the heap. Clear destroys what is left and rewinds the arena; the arena
     * keeps its pages, so a steady tick takes no new memory.
     */
    class DeferredCommands
    {
    public:
        /** A deferred change to the storage: a callable in the arena. */
        struct StructuralFn
        {
            void (*Invoke)(void* callable, EntityStorage& storage) = nullptr;
            void (*Destroy)(void* callable) = nullptr;
            void* Callable = nullptr;

            void operator()(EntityStorage& storage) const { Invoke(Callable, storage); }
        };

        struct ComponentChange
        {
            Entity Target;
            Detail::PendingChange Change;
        };

        DeferredCommands() = default;
        DeferredCommands(const DeferredCommands&) = delete;
        DeferredCommands& operator=(const DeferredCommands&) = delete;
        DeferredCommands(DeferredCommands&& other) noexcept
            : m_Destroy(std::move(other.m_Destroy)),
              m_Structural(std::move(other.m_Structural)),
              m_Changes(std::move(other.m_Changes)),
              m_Arena(std::move(other.m_Arena))
        {
            other.Forget();
        }

        /** Destroys this buffer's own unapplied values, then takes the other buffer's. */
        DeferredCommands& operator=(DeferredCommands&& other) noexcept
        {
            if (this != &other)
            {
                Clear();
                m_Destroy = std::move(other.m_Destroy);
                m_Structural = std::move(other.m_Structural);
                m_Changes = std::move(other.m_Changes);
                m_Arena = std::move(other.m_Arena);
                other.Forget();
            }
            return *this;
        }

        ~DeferredCommands() { Clear(); }

        /**
         * Destroys the entity, and its children, at the end of the tick. Destruction is
         * deferred, so handles stay valid during the tick.
         */
        void Destroy(const Entity entity) { m_Destroy.push_back(entity); }

        std::span<const Entity> Destroys() const { return m_Destroy; }

        /** Add (or replace) a component at the flush. Use the typed context call ctx.Add. */
        template <typename T, typename V>
        void AddComponent(const Entity entity, V&& value)
        {
            // A reference, not a copy: MSVC does not take a parameter aligned to more than 64 bytes by value
            void* memory = m_Arena.Allocate(sizeof(T), alignof(T));
            T* object = ::new (memory) T(std::forward<V>(value));
            Detail::PendingChange change;
            change.Id = Detail::ComponentIdOf<T>();
            change.IsAdd = true;
            change.Value = object;
            change.Destroy = [](void* p)
            {
                static_cast<T*>(p)->~T();
            };
            m_Changes.push_back(ComponentChange{entity, change});
        }

        /** Remove a component at the flush, if the entity has it. Use the typed context call ctx.Remove. */
        template <typename T>
        void RemoveComponent(const Entity entity)
        {
            Detail::PendingChange change;
            change.Id = Detail::ComponentIdOf<T>();
            change.IsAdd = false;
            m_Changes.push_back(ComponentChange{entity, change});
        }

        std::span<ComponentChange> ComponentChanges() { return m_Changes; }

        /** A change to the storage at the flush: fn(EntityStorage&). Use the typed context call ctx.Defer. */
        template <typename Fn>
        void Structural(Fn&& fn)
        {
            using Callable = std::decay_t<Fn>;
            void* memory = m_Arena.Allocate(sizeof(Callable), alignof(Callable));
            ::new (memory) Callable(std::forward<Fn>(fn));
            m_Structural.push_back(StructuralFn{[](void* callable, EntityStorage& storage)
                                                { (*static_cast<Callable*>(callable))(storage); },
                                                [](void* callable) { static_cast<Callable*>(callable)->~Callable(); },
                                                memory});
        }

        std::span<const StructuralFn> StructuralChanges() const { return m_Structural; }

        bool Empty() const { return m_Destroy.empty() && m_Structural.empty() && m_Changes.empty(); }

        /** Drop everything: destroy the values that were not applied, and the callables, then rewind the arena. */
        void Clear()
        {
            for (ComponentChange& each : m_Changes)
            {
                if (each.Change.Value != nullptr)
                {
                    each.Change.Destroy(each.Change.Value);
                }
            }
            for (const StructuralFn& each : m_Structural)
            {
                each.Destroy(each.Callable);
            }
            m_Changes.clear();
            m_Destroy.clear();
            m_Structural.clear();
            m_Arena.Reset();
        }

        /** Bytes the arena took from the OS. */
        size_t ReservedBytes() const { return m_Arena.ReservedBytes(); }

    private:
        /** Empty the lists without destroying anything: another buffer owns them now. */
        void Forget()
        {
            m_Destroy.clear();
            m_Structural.clear();
            m_Changes.clear();
        }

        std::vector<Entity> m_Destroy;
        std::vector<StructuralFn> m_Structural;
        std::vector<ComponentChange> m_Changes;
        LinearArena m_Arena;
    };

    /** The storage a system's OnCreate and OnDestroy see. Systems use SystemSetup<Access> instead. */
    class SystemSetupBase
    {
    public:
        explicit SystemSetupBase(EntityStorage& storage) : m_Storage(storage) {}

        /** The storage. Only Ecs/ code uses it. */
        EntityStorage& Storage() const { return m_Storage; }

    private:
        EntityStorage& m_Storage;
    };

    /** The storage and command buffer one system sees in one call. Systems use their typed Context instead. */
    class SystemContextBase
    {
    public:
        SystemContextBase(EntityStorage& storage, DeferredCommands* commands, const TickInput* tickInput = nullptr)
            : m_Storage(storage), m_Commands(commands), m_TickInput(tickInput)
        {
        }

        /** The storage. Only Ecs/ code uses it. */
        EntityStorage& Storage() const { return m_Storage; }

        /** Null in a variable rate phase. */
        DeferredCommands* Commands() const { return m_Commands; }

        /** This tick's input. Null in a variable rate phase. */
        const TickInput* GetTickInput() const { return m_TickInput; }

    private:
        EntityStorage& m_Storage;
        DeferredCommands* m_Commands = nullptr;
        const TickInput* m_TickInput = nullptr;
    };

    /** Pushes requests into one queue. A queue component keeps its requests in a container named Requests. */
    template <typename Q>
    class QueueWriter
    {
    public:
        explicit QueueWriter(Q& queue) : m_Queue(queue) {}

        template <typename R>
        void Push(R&& request)
        {
            m_Queue.Requests.push_back(std::forward<R>(request));
        }

    private:
        Q& m_Queue;
    };

    /**
     * What a system's OnCreate and OnDestroy may do. It makes and removes the
     * singletons that the system owns: a singleton it writes, or a queue it consumes.
     * The engine makes those already, with their default values, so a system uses this
     * only to give one other values (AddSingleton replaces it).
     */
    template <typename A>
    class SystemSetup
    {
    public:
        explicit SystemSetup(SystemSetupBase& core) : m_Core(core) {}

        SystemSetup(const SystemSetup&) = delete;
        SystemSetup& operator=(const SystemSetup&) = delete;

        /** Makes the singleton T, or replaces it. */
        template <typename T, typename... Args>
        T& AddSingleton(Args&&... args)
        {
            CheckOwner<T>();
            return m_Core.Storage().template SetSingleton<T>(std::forward<Args>(args)...);
        }

        /** Removes the singleton T. */
        template <typename T>
        void RemoveSingleton()
        {
            CheckOwner<T>();
            m_Core.Storage().template RemoveSingleton<T>();
        }

    private:
        template <typename T>
        static constexpr void CheckOwner()
        {
            static_assert(Component<T> && IsSingletonComponent<T>,
                          "AddSingleton and RemoveSingleton need a Singleton "
                          "or Queue component.");
            if constexpr (Component<T> && ComponentInfoOf<T>.Kind == ComponentKind::Queue)
            {
                static_assert(Allows<A, AccessMode::Consume, T>,
                              "Only the consumer of a queue makes it. Add Consume<Q> to the access list.");
            }
            else
            {
                static_assert(Allows<A, AccessMode::Write, T>,
                              "Only a writer of a singleton makes it. Add Write<T> to the access list.");
            }
        }

        SystemSetupBase& m_Core;
    };

    /**
     * The world as one system sees it: only what its access list declares. A use
     * that the list does not allow does not compile. The adapter makes one on the
     * stack for each call, so it costs nothing to make.
     *
     * Spawn, Commands, Queue, and Drain exist only in a fixed phase. A frame rate
     * system that wants to change the simulation sends a command that the next tick reads.
     */
    template <typename Derived, typename A>
    class SystemContext
    {
    public:
        explicit SystemContext(SystemContextBase& core) : m_Core(core) {}

        SystemContext(const SystemContext&) = delete;
        SystemContext& operator=(const SystemContext&) = delete;

        /**
         * Every entity that has each component in Ts, with the filters applied:
         * `ctx.Query<Transform, const Velocity>(Fling::Without<Frozen>{})`.
         * T needs Write<T>. const T needs Read<T> or Write<T>.
         */
        template <typename... Ts, typename... Filters>
        auto Query(Filters...)
        {
            static_assert(sizeof...(Ts) > 0, "Query needs at least one component to return. Use With<T> to filter.");
            static_assert((Detail::IsTableComponent<Ts> && ...),
                          "Query returns Table components only. Use With<Tag> to filter by a tag, and Singleton<T>() "
                          "for a singleton.");
            static_assert((CanUse<Ts>() && ...),
                          "This system queries a component that its Access<...> does not "
                          "allow. Add Write<T>, or Read<T> and query const T.");
            static_assert(Detail::AreFilters<Filters...>,
                          "Query takes only Fling::With<...> and Fling::Without<...> as arguments.");

            using Withs = Detail::WithTypes<Filters...>;
            using Withouts = Detail::WithoutTypes<Filters...>;
            CheckFilters(Withs{});
            CheckFilters(Withouts{});
            static_assert(AllUnique<std::remove_const_t<Ts>...>, "A component appears twice in one Query.");

            return MakeRange<TypeList<Ts...>>(Withs{}, Withouts{});
        }

        /** The component T on one entity. The entity must be alive and have T. T needs Write<T>; const T needs Read<T>.
         */
        template <typename T>
        T& Get(const Entity entity)
        {
            T* component = TryGet<T>(entity);
            assert(component != nullptr && "Get: the entity is gone or does not have this component");
            return *component;
        }

        /** The component T on one entity, or null if the entity is gone or does not have T. Use it to follow a
         * reference. */
        template <typename T>
        T* TryGet(const Entity entity)
        {
            static_assert(Detail::IsTableComponent<T>, "Get and TryGet return Table components only.");
            static_assert(CanUse<T>(),
                          "This system gets a component that its Access<...> does not allow. Add "
                          "Write<T>, or Read<T> and get const T.");

            // Null for a dead entity too
            return m_Core.Storage().template TryGet<std::remove_const_t<T>>(entity);
        }

        /** True if the entity is alive. */
        bool IsAlive(const Entity entity) const { return m_Core.Storage().IsValid(entity); }

        /**
         * True if the entity is alive and has T. Use it for a Tag, which TryGet cannot
         * return. T needs Read<T> (or Write<T>).
         */
        template <typename T>
        bool Has(const Entity entity) const
        {
            using Type = std::remove_const_t<T>;
            static_assert(Detail::IsFilterComponent<Type>, "Has checks Table and Tag components only.");
            static_assert(CanUse<const Type>(),
                          "This system checks a component that its Access<...> does not allow. Add Read<T>.");

            // False for a dead entity too
            return static_cast<const EntityStorage&>(m_Core.Storage()).template Has<Type>(entity);
        }

        /** The singleton T. T needs Write<T>; const T needs Read<T>. The owner makes it in OnCreate. */
        template <typename T>
        T& Singleton()
        {
            using U = std::remove_const_t<T>;
            static_assert(Component<U> && ComponentInfoOf<U>.Kind == ComponentKind::Singleton,
                          "Singleton<T> needs a component with Kind = ComponentKind::Singleton. Use Queue<Q> and "
                          "Drain<Q> for a queue.");
            static_assert(CanUse<T>(),
                          "This system uses a singleton that its Access<...> does not allow. Add "
                          "Write<T>, or Read<T> and use const T.");

            U* singleton = m_Core.Storage().template TryGetSingleton<U>();
            assert(singleton != nullptr &&
                   "Singleton: nobody made it. The engine makes it for a system that writes it, unless it is External");
            return *singleton;
        }

        /** Pushes requests into the queue Q. Needs Append<Q>. Fixed phases only. */
        template <typename Q>
        QueueWriter<Q> Queue()
        {
            static_assert(IsFixed(Derived::Info.Phase),
                          "Only a fixed phase system appends to a queue. A variable rate system cannot change the "
                          "simulation.");
            static_assert(Allows<A, AccessMode::Append, Q>, "Add Append<Q> to the access list to push into Q.");
            return QueueWriter<Q>(QueueComponent<Q>());
        }

        /** Takes every request out of the queue Q and leaves it empty. Needs Consume<Q>. Fixed phases only. */
        template <typename Q>
        auto Drain()
        {
            static_assert(IsFixed(Derived::Info.Phase), "Only a fixed phase system consumes a queue.");
            static_assert(Allows<A, AccessMode::Consume, Q>, "Add Consume<Q> to the access list to drain Q.");
            return std::exchange(QueueComponent<Q>().Requests, {});
        }

        /**
         * Makes an entity now, with the given components. Creation is immediate, so the
         * handle works this tick. Needs Spawns, and Write<T> for each component. A
         * loop that is running when the entity is spawned does not see it.
         */
        template <typename... Cs>
        Entity Spawn(Cs&&... components)
        {
            static_assert(IsFixed(Derived::Info.Phase), "Only a fixed phase system spawns entities.");
            static_assert(A::HasSpawns, "Add Fling::Spawns to the access list to spawn entities.");
            static_assert((Allows<A, AccessMode::Write, std::decay_t<Cs>> && ...),
                          "Spawn adds a component that the access list does not write. Add Write<T>.");

            if constexpr (sizeof...(Cs) == 0)
            {
                return m_Core.Storage().Create();
            }
            else
            {
                return m_Core.Storage().Create(std::forward<Cs>(components)...);
            }
        }

        /**
         * The storage itself, for a bridge to a specialist subsystem that works on it
         * (Physics.Step calls Physics::Tick). Needs Fling::RawStorage in the access list,
         * which must still name every type the subsystem touches. Fixed phases only.
         */
        EntityStorage& RawStorage()
        {
            static_assert(IsFixed(Derived::Info.Phase), "Only a fixed phase system uses RawStorage.");
            static_assert(A::HasRawStorage, "Add Fling::RawStorage to the access list to use ctx.RawStorage().");
            return m_Core.Storage();
        }

        /**
         * The input of this tick, merged over devices, with presses and releases counted.
         * Fixed phases only. Only the engine's Input system reads it: it copies it into
         * the InputState singleton, which gameplay reads.
         */
        const TickInput& ReadTickInput() const
        {
            static_assert(IsFixed(Derived::Info.Phase),
                          "Only a fixed phase system reads the tick input. Read InputState instead.");
            static const TickInput s_NoInput{};
            return m_Core.GetTickInput() != nullptr ? *m_Core.GetTickInput() : s_NoInput;
        }

        /**
         * Add (or replace) the entity's T at the end of the tick. Needs Write<T>. A stale
         * handle is ignored at the flush. Fixed phases only.
         */
        template <typename T>
        void Add(const Entity entity, T&& value)
        {
            using Type = std::remove_cvref_t<T>;
            static_assert(Allows<A, AccessMode::Write, Type>, "Add needs Write<T> in the access list.");
            Commands().template AddComponent<Type>(entity, std::forward<T>(value));
        }

        /** Remove the entity's T, if it has one, at the end of the tick. Needs Write<T>. Fixed phases only. */
        template <typename T>
        void Remove(const Entity entity)
        {
            static_assert(Allows<A, AccessMode::Write, T>, "Remove needs Write<T> in the access list.");
            Commands().template RemoveComponent<T>(entity);
        }

        /**
         * Run fn on the storage at the end of the tick, for a change that keeps several
         * components in step (HierarchyUtil::SetParent). Ws are every type fn writes, and
         * each needs Write<W>. Fixed phases only.
         */
        template <typename... Ws, typename Fn>
        void Defer(Fn&& fn)
        {
            static_assert(sizeof...(Ws) > 0, "Name each type that the deferred change writes.");
            static_assert((Allows<A, AccessMode::Write, Ws> && ...),
                          "Defer writes a type that the access list does not write. Add Write<T>.");
            Commands().Structural(std::forward<Fn>(fn));
        }

        /** Changes that wait until the end of the tick, such as Destroy. Fixed phases only. */
        DeferredCommands& Commands()
        {
            static_assert(IsFixed(Derived::Info.Phase),
                          "Only a fixed phase system has Commands. A variable rate system cannot change the "
                          "simulation.");
            assert(m_Core.Commands() != nullptr);
            return *m_Core.Commands();
        }

    private:
        /** True if the access list allows T (Write) or const T (Read). */
        template <typename T>
        static constexpr bool CanUse()
        {
            if constexpr (std::is_const_v<T>)
            {
                return Allows<A, AccessMode::Read, T>;
            }
            else
            {
                return Allows<A, AccessMode::Write, T>;
            }
        }

        template <typename... Fs>
        static constexpr void CheckFilters(TypeList<Fs...>)
        {
            static_assert((Detail::IsFilterComponent<Fs> && ...),
                          "With<T> and Without<T> take Table or Tag components, with no const.");
            static_assert((Allows<A, AccessMode::Read, Fs> && ...),
                          "A With<T> or Without<T> filter needs Read<T> (or Write<T>) in the access list.");
        }

        template <typename Gets, typename... Ws, typename... Xs>
        QueryRange<Gets, TypeList<Ws...>, TypeList<Xs...>> MakeRange(TypeList<Ws...>, TypeList<Xs...>)
        {
            return QueryRange<Gets, TypeList<Ws...>, TypeList<Xs...>>(m_Core.Storage().Native());
        }

        template <typename Q>
        Q& QueueComponent()
        {
            Q* queue = m_Core.Storage().template TryGetSingleton<Q>();
            assert(queue != nullptr &&
                   "Queue: nobody made it. The engine makes it for the system that consumes it, unless it is External");
            return *queue;
        }

        SystemContextBase& m_Core;
    };

    namespace Detail
    {
        /**
         * True if a system with the access item E owns the type: it writes a singleton, or
         * consumes a queue, that the engine can make (not External, and with a default
         * constructor).
         */
        template <typename E>
        consteval bool OwnsSingleton()
        {
            if constexpr (std::is_base_of_v<AccessItemBase, E>)
            {
                using T = typename E::Type;
                const ComponentInfo info = ComponentInfoOf<T>;
                const bool owner = (E::Mode == AccessMode::Write && info.Kind == ComponentKind::Singleton) ||
                                   (E::Mode == AccessMode::Consume && info.Kind == ComponentKind::Queue);
                return owner && !info.External && std::is_default_constructible_v<T>;
            }
            return false;
        }

        template <typename E>
        void PrepareOne(EntityStorage& storage, std::vector<void (*)(EntityStorage&)>& made)
        {
            if constexpr (std::is_base_of_v<AccessItemBase, E>)
            {
                // Give the type its ComponentId before the first tick, not in the middle of one
                (void)ComponentIdOf<typename E::Type>();
            }
            if constexpr (OwnsSingleton<E>())
            {
                using T = typename E::Type;
                // Another writer, or code outside the systems, can have made it already. Then it is not ours to remove.
                if (storage.template TryGetSingleton<T>() == nullptr)
                {
                    storage.template SetSingleton<T>();
                    made.push_back([](EntityStorage& s) { s.template RemoveSingleton<T>(); });
                }
            }
        }

        template <typename... Es>
        void PrepareAll(EntityStorage& storage, std::vector<void (*)(EntityStorage&)>& made, TypeList<Es...>)
        {
            (PrepareOne<Es>(storage, made), ...);
        }

        /** What the runner does for a system with the access list A before any OnCreate. See SystemDesc::Prepare. */
        template <typename A>
        void PrepareSystem(EntityStorage& storage, std::vector<void (*)(EntityStorage&)>& made)
        {
            PrepareAll(storage, made, typename A::Items{});
        }
    } // namespace Detail
} // namespace Fling
