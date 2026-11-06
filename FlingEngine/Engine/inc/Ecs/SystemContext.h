#pragma once

#include "Ecs/Access.h"
#include "Ecs/Component.h"
#include "Ecs/detail/EnttBridge.h"
#include "Ecs/Entity.h"
#include "Ecs/Phase.h"
#include "Ecs/TypeList.h"

#include <cassert>
#include <cstddef>
#include <span>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fling
{
    /** Changes that wait until the end of the fixed tick. Each system has its own buffer. */
    class CommandBuffer
    {
    public:
        /** Destroys the entity at the end of the tick. Destruction is deferred, so handles stay valid during the tick.
         */
        void Destroy(const Entity entity) { m_Destroy.push_back(entity); }

        std::span<const Entity> Destroys() const { return m_Destroy; }

        bool Empty() const { return m_Destroy.empty(); }

        void Clear() { m_Destroy.clear(); }

    private:
        std::vector<Entity> m_Destroy;
    };

    /** The storage a system's OnCreate and OnDestroy see. Systems use SystemSetup<Access> instead. */
    class SystemSetupBase
    {
    public:
        explicit SystemSetupBase(entt::registry& registry) : m_Registry(registry) {}

        /** The storage. Only Ecs/ code uses it. */
        entt::registry& Storage() const { return m_Registry; }

    private:
        entt::registry& m_Registry;
    };

    /** The storage and command buffer one system sees in one call. Systems use their typed Context instead. */
    class SystemContextBase
    {
    public:
        SystemContextBase(entt::registry& registry, CommandBuffer* commands)
            : m_Registry(registry), m_Commands(commands)
        {
        }

        /** The storage. Only Ecs/ code uses it. */
        entt::registry& Storage() const { return m_Registry; }

        /** Null in a frame phase. */
        CommandBuffer* Commands() const { return m_Commands; }

    private:
        entt::registry& m_Registry;
        CommandBuffer* m_Commands = nullptr;
    };

    /** A query filter: only entities that also have each T. The components are not returned. */
    template <typename... Ts>
    struct With
    {
    };

    /** A query filter: only entities that have none of the Ts. */
    template <typename... Ts>
    struct Without
    {
    };

    namespace Detail
    {
        template <typename F>
        struct FilterTypes
        {
            static constexpr bool IsFilter = false;
        };

        template <typename... Ts>
        struct FilterTypes<With<Ts...>>
        {
            static constexpr bool IsFilter = true;
            using WithList = TypeList<Ts...>;
            using WithoutList = TypeList<>;
        };

        template <typename... Ts>
        struct FilterTypes<Without<Ts...>>
        {
            static constexpr bool IsFilter = true;
            using WithList = TypeList<>;
            using WithoutList = TypeList<Ts...>;
        };

        template <typename... Lists>
        struct Concat
        {
            using Type = TypeList<>;
        };

        template <typename... As>
        struct Concat<TypeList<As...>>
        {
            using Type = TypeList<As...>;
        };

        template <typename... As, typename... Bs, typename... Rest>
        struct Concat<TypeList<As...>, TypeList<Bs...>, Rest...>
        {
            using Type = typename Concat<TypeList<As..., Bs...>, Rest...>::Type;
        };

        template <typename T>
        inline constexpr bool IsTableComponent =
            Component<std::remove_const_t<T>> && ComponentInfoOf<std::remove_const_t<T>>.Kind == ComponentKind::Table;

        template <typename T>
        inline constexpr bool IsFilterComponent = Component<T> && (ComponentInfoOf<T>.Kind == ComponentKind::Table ||
                                                                   ComponentInfoOf<T>.Kind == ComponentKind::Tag);
    } // namespace Detail

    template <typename Gets, typename Withs, typename Withouts>
    class QueryRange;

    /**
     * The entities that have every component in Ts and pass the filters. Each item is
     * std::tuple<Entity, T&...>, so write `for (auto [entity, a, b] : ctx.Query<A, const B>())`.
     */
    template <typename... Ts, typename... Ws, typename... Xs>
    class QueryRange<TypeList<Ts...>, TypeList<Ws...>, TypeList<Xs...>>
    {
        using View = entt::basic_view<entt::entity, std::remove_const_t<Ts>..., Ws...>;
        using ViewIterator = decltype(std::declval<const View&>().begin());

    public:
        using Item = std::tuple<Entity, Ts&...>;

        class Iterator
        {
        public:
            Iterator(entt::registry& registry, ViewIterator it, ViewIterator end)
                : m_Registry(&registry), m_It(it), m_End(end)
            {
                SkipExcluded();
            }

            Item operator*() const
            {
                const entt::entity e = *m_It;
                return Item{Detail::FromEntt(e), m_Registry->template get<std::remove_const_t<Ts>>(e)...};
            }

            Iterator& operator++()
            {
                ++m_It;
                SkipExcluded();
                return *this;
            }

            bool operator==(const Iterator& other) const { return m_It == other.m_It; }

            bool operator!=(const Iterator& other) const { return m_It != other.m_It; }

        private:
            void SkipExcluded()
            {
                if constexpr (sizeof...(Xs) > 0)
                {
                    while (m_It != m_End && (m_Registry->template has<Xs>(*m_It) || ...))
                    {
                        ++m_It;
                    }
                }
            }

            entt::registry* m_Registry;
            ViewIterator m_It;
            ViewIterator m_End;
        };

        explicit QueryRange(entt::registry& registry)
            : m_Registry(registry), m_View(registry.template view<std::remove_const_t<Ts>..., Ws...>())
        {
        }

        Iterator begin() const { return Iterator(m_Registry, m_View.begin(), m_View.end()); }

        Iterator end() const { return Iterator(m_Registry, m_View.end(), m_View.end()); }

    private:
        entt::registry& m_Registry;
        View m_View;
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
            return m_Core.Storage().template set<T>(std::forward<Args>(args)...);
        }

        /** Removes the singleton T. */
        template <typename T>
        void RemoveSingleton()
        {
            CheckOwner<T>();
            m_Core.Storage().template unset<T>();
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
            static_assert((Detail::FilterTypes<Filters>::IsFilter && ...),
                          "Query takes only Fling::With<...> and Fling::Without<...> as arguments.");

            using Withs = typename Detail::Concat<typename Detail::FilterTypes<Filters>::WithList...>::Type;
            using Withouts = typename Detail::Concat<typename Detail::FilterTypes<Filters>::WithoutList...>::Type;
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

            entt::registry& registry = m_Core.Storage();
            const entt::entity e = Detail::ToEntt(entity);
            if (e == entt::null || !registry.valid(e))
            {
                return nullptr;
            }
            return registry.template try_get<std::remove_const_t<T>>(e);
        }

        /** True if the entity is alive. */
        bool IsAlive(const Entity entity) const
        {
            const entt::entity e = Detail::ToEntt(entity);
            return e != entt::null && m_Core.Storage().valid(e);
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

            U* singleton = m_Core.Storage().template try_ctx<U>();
            assert(singleton != nullptr && "Singleton: nobody made it. Its writer makes it in OnCreate");
            return *singleton;
        }

        /** Pushes requests into the queue Q. Needs Append<Q>. Fixed phases only. */
        template <typename Q>
        QueueWriter<Q> Queue()
        {
            static_assert(IsFixedPhase(Derived::Info.Phase),
                          "Only a fixed phase system appends to a queue. A frame rate system cannot change the "
                          "simulation.");
            static_assert(Allows<A, AccessMode::Append, Q>, "Add Append<Q> to the access list to push into Q.");
            return QueueWriter<Q>(QueueComponent<Q>());
        }

        /** Takes every request out of the queue Q and leaves it empty. Needs Consume<Q>. Fixed phases only. */
        template <typename Q>
        auto Drain()
        {
            static_assert(IsFixedPhase(Derived::Info.Phase), "Only a fixed phase system consumes a queue.");
            static_assert(Allows<A, AccessMode::Consume, Q>, "Add Consume<Q> to the access list to drain Q.");
            return std::exchange(QueueComponent<Q>().Requests, {});
        }

        /**
         * Makes an entity now, with the given components. Creation is immediate, so the
         * handle works this tick. Needs Spawns, and Write<T> for each component. Do
         * not spawn a component type that a loop in this call is iterating.
         */
        template <typename... Cs>
        Entity Spawn(Cs&&... components)
        {
            static_assert(IsFixedPhase(Derived::Info.Phase), "Only a fixed phase system spawns entities.");
            static_assert(A::HasSpawns, "Add Fling::Spawns to the access list to spawn entities.");
            static_assert((Allows<A, AccessMode::Write, std::decay_t<Cs>> && ...),
                          "Spawn adds a component that the access list does not write. Add Write<T>.");

            entt::registry& registry = m_Core.Storage();
            const entt::entity e = registry.create();
            (registry.template assign<std::decay_t<Cs>>(e, std::forward<Cs>(components)), ...);
            return Detail::FromEntt(e);
        }

        /** Changes that wait until the end of the tick, such as Destroy. Fixed phases only. */
        CommandBuffer& Commands()
        {
            static_assert(IsFixedPhase(Derived::Info.Phase),
                          "Only a fixed phase system has Commands. A frame rate system cannot change the "
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
            return QueryRange<Gets, TypeList<Ws...>, TypeList<Xs...>>(m_Core.Storage());
        }

        template <typename Q>
        Q& QueueComponent()
        {
            Q* queue = m_Core.Storage().template try_ctx<Q>();
            assert(queue != nullptr && "Queue: nobody made it. Its consumer makes it in OnCreate");
            return *queue;
        }

        SystemContextBase& m_Core;
    };

    namespace Detail
    {
        template <typename E>
        void PrepareOne(entt::registry& registry)
        {
            if constexpr (std::is_base_of_v<AccessItemBase, E>)
            {
                using T = typename E::Type;
                if constexpr (!IsSingletonComponent<T>)
                {
                    registry.template reserve<T>(0);
                }
            }
        }

        template <typename... Es>
        void PrepareAll(entt::registry& registry, TypeList<Es...>)
        {
            (PrepareOne<Es>(registry), ...);
        }

        /** Makes the EnTT storage for each Table and Tag component in A. EnTT 3.1 would make it on first use otherwise.
         */
        template <typename A>
        void PrepareStorage(SystemSetupBase& setup)
        {
            PrepareAll(setup.Storage(), typename A::Items{});
        }
    } // namespace Detail
} // namespace Fling
