#pragma once

#include "Ecs/Access.h"
#include "Ecs/Component.h"
#include "Ecs/Entity.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/Phase.h"
#include "Ecs/TypeList.h"
#include "Input/TickInput.h"

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
     */
    class DeferredCommands
    {
    public:
        using StructuralFn = std::function<void(EntityStorage&)>;

        struct ComponentChange
        {
            Entity Target;
            Detail::PendingChange Change;
        };

        DeferredCommands() = default;
        DeferredCommands(const DeferredCommands&) = delete;
        DeferredCommands& operator=(const DeferredCommands&) = delete;
        DeferredCommands(DeferredCommands&& other) noexcept = default;
        DeferredCommands& operator=(DeferredCommands&& other) noexcept = default;
        ~DeferredCommands() { Clear(); }

        /**
         * Destroys the entity, and its children, at the end of the tick. Destruction is
         * deferred, so handles stay valid during the tick.
         */
        void Destroy(const Entity entity) { m_Destroy.push_back(entity); }

        std::span<const Entity> Destroys() const { return m_Destroy; }

        /** Add (or replace) a component at the flush. Use the typed context call ctx.Add. */
        template <typename T>
        void AddComponent(const Entity entity, T value)
        {
            m_Changes.push_back(ComponentChange{entity,
                                                Detail::PendingChange{Detail::ComponentIdOf<T>(),
                                                                      true,
                                                                      new T(std::move(value)),
                                                                      [](void* object)
                                                                      {
                                                                          delete static_cast<T*>(object);
                                                                      }}});
        }

        /** Remove a component at the flush, if the entity has it. Use the typed context call ctx.Remove. */
        template <typename T>
        void RemoveComponent(const Entity entity)
        {
            m_Changes.push_back(
                ComponentChange{entity, Detail::PendingChange{Detail::ComponentIdOf<T>(), false, nullptr, nullptr}});
        }

        std::span<ComponentChange> ComponentChanges() { return m_Changes; }

        /** A change to the storage at the flush. Use the typed context call ctx.Defer. */
        void Structural(StructuralFn fn) { m_Structural.push_back(std::move(fn)); }

        std::span<const StructuralFn> StructuralChanges() const { return m_Structural; }

        bool Empty() const { return m_Destroy.empty() && m_Structural.empty() && m_Changes.empty(); }

        /** Drop everything. Values of adds that were not applied are freed. */
        void Clear()
        {
            for (ComponentChange& each : m_Changes)
            {
                if (each.Change.Value != nullptr)
                {
                    each.Change.Free(each.Change.Value);
                }
            }
            m_Changes.clear();
            m_Destroy.clear();
            m_Structural.clear();
        }

    private:
        std::vector<Entity> m_Destroy;
        std::vector<StructuralFn> m_Structural;
        std::vector<ComponentChange> m_Changes;
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

        /** Null in a frame phase. */
        DeferredCommands* Commands() const { return m_Commands; }

        /** This tick's input. Null in a frame phase. */
        const TickInput* GetTickInput() const { return m_TickInput; }

    private:
        EntityStorage& m_Storage;
        DeferredCommands* m_Commands = nullptr;
        const TickInput* m_TickInput = nullptr;
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
     *
     * It walks the matching archetypes chunk by chunk over raw column pointers. The
     * archetypes, chunks, and rows to visit are fixed when each one is reached, so an
     * entity spawned during the loop is not seen by this loop (later systems see it).
     */
    template <typename... Ts, typename... Ws, typename... Xs>
    class QueryRange<TypeList<Ts...>, TypeList<Ws...>, TypeList<Xs...>>
    {
        static constexpr size_t GetCount = sizeof...(Ts);

        template <typename T>
        static constexpr bool IsSparse = ComponentInfoOf<std::remove_const_t<T>>.Storage == ComponentStorage::Sparse;

        template <typename T>
        static constexpr bool IsEnableable =
            ComponentInfoOf<std::remove_const_t<T>>.Storage == ComponentStorage::Enableable;

    public:
        using Item = std::tuple<Entity, Ts&...>;

        class Iterator
        {
        public:
            Iterator() = default;

            Iterator(const QueryRange* range, const bool atEnd) : m_Range(range)
            {
                if (atEnd || m_Range->m_Archetypes.empty())
                {
                    m_ArchIndex = m_Range ? m_Range->m_Archetypes.size() : 0;
                    return;
                }
                EnterArchetype();
                SkipToValid();
            }

            Item operator*() const { return MakeItem(std::index_sequence_for<Ts...>{}); }

            Iterator& operator++()
            {
                ++m_Row;
                SkipToValid();
                return *this;
            }

            bool operator==(const Iterator& other) const
            {
                return m_ArchIndex == other.m_ArchIndex && m_ChunkIndex == other.m_ChunkIndex && m_Row == other.m_Row;
            }

            bool operator!=(const Iterator& other) const { return !(*this == other); }

        private:
            template <size_t... I>
            Item MakeItem(std::index_sequence<I...>) const
            {
                const Entity entity = m_Chunk->Entities[m_Row];
                return Item{entity, Column<Ts, I>(entity)...};
            }

            template <typename T, size_t I>
            T& Column(const Entity entity) const
            {
                if constexpr (IsSparse<T>)
                {
                    return *static_cast<T*>(m_Range->m_Storage->Get(entity, m_Range->m_GetIds[I]));
                }
                else
                {
                    return *reinterpret_cast<T*>(m_Columns[I] + m_Row * sizeof(std::remove_const_t<T>));
                }
            }

            void EnterArchetype()
            {
                m_Arch = m_Range->m_Archetypes[m_ArchIndex];
                m_ChunkCount = m_Arch->Chunks.size();
                m_ChunkIndex = 0;
                EnterChunk();
            }

            void EnterChunk()
            {
                m_Row = 0;
                if (m_ChunkIndex >= m_ChunkCount || m_ChunkIndex >= m_Arch->Chunks.size())
                {
                    m_Chunk = nullptr;
                    m_RowCount = 0;
                    return;
                }
                m_Chunk = m_Arch->Chunks[m_ChunkIndex].get();
                m_RowCount = m_Chunk->Count;
                for (size_t i = 0; i < GetCount; ++i)
                {
                    const int32 column = m_Arch->ColumnOf(m_Range->m_GetIds[i]);
                    m_Columns[i] = column >= 0 ? m_Chunk->Columns[static_cast<size_t>(column)] : nullptr;
                }
            }

            /** Move to the next row that passes the per-row checks (Sparse and Enableable types). */
            void SkipToValid()
            {
                while (m_ArchIndex < m_Range->m_Archetypes.size())
                {
                    while (m_Chunk != nullptr && m_Row < m_RowCount)
                    {
                        if (RowPasses())
                        {
                            return;
                        }
                        ++m_Row;
                    }
                    if (m_Chunk != nullptr && m_ChunkIndex + 1 < m_ChunkCount)
                    {
                        ++m_ChunkIndex;
                        EnterChunk();
                        continue;
                    }
                    ++m_ArchIndex;
                    if (m_ArchIndex < m_Range->m_Archetypes.size())
                    {
                        EnterArchetype();
                    }
                }
                // At the end: the same state as end()
                m_ChunkIndex = 0;
                m_Row = 0;
            }

            bool RowPasses() const
            {
                const QueryRange& range = *m_Range;
                if (range.m_PerRowChecks)
                {
                    const Entity entity = m_Chunk->Entities[m_Row];
                    for (const Detail::ComponentId id : range.m_SparseRequired)
                    {
                        if (!range.m_Storage->Has(entity, id))
                        {
                            return false;
                        }
                    }
                    for (const Detail::ComponentId id : range.m_SparseExcluded)
                    {
                        if (range.m_Storage->Has(entity, id))
                        {
                            return false;
                        }
                    }
                    for (const Detail::ComponentId id : range.m_EnableableRequired)
                    {
                        const int32 column = m_Arch->ColumnOf(id);
                        if (column >= 0 &&
                            !Detail::NativeStorage::RowEnabled(*m_Chunk, static_cast<size_t>(column), m_Row))
                        {
                            return false;
                        }
                    }
                }
                return true;
            }

            const QueryRange* m_Range = nullptr;
            size_t m_ArchIndex = 0;
            Detail::Archetype* m_Arch = nullptr;
            size_t m_ChunkIndex = 0;
            size_t m_ChunkCount = 0;
            Detail::Chunk* m_Chunk = nullptr;
            uint32 m_Row = 0;
            uint32 m_RowCount = 0;
            std::array<std::byte*, GetCount == 0 ? 1 : GetCount> m_Columns{};
        };

        explicit QueryRange(Detail::NativeStorage& storage)
            : m_Storage(&storage), m_GetIds{Detail::ComponentIdOf<std::remove_const_t<Ts>>()...}
        {
            std::vector<Detail::ComponentId> required;
            std::vector<Detail::ComponentId> excluded;
            (Classify<std::remove_const_t<Ts>>(required, m_SparseRequired), ...);
            (Classify<Ws>(required, m_SparseRequired), ...);
            (Classify<Xs>(excluded, m_SparseExcluded), ...);
            (AddEnableable<std::remove_const_t<Ts>>(), ...);
            (AddEnableable<Ws>(), ...);
            std::sort(required.begin(), required.end());
            std::sort(excluded.begin(), excluded.end());
            m_PerRowChecks = !m_SparseRequired.empty() || !m_SparseExcluded.empty() || !m_EnableableRequired.empty();
            // A copy, so archetypes made during the loop are not visited by it
            m_Archetypes = storage.Match(required, excluded);
        }

        Iterator begin() const { return Iterator(this, false); }

        Iterator end() const { return Iterator(this, true); }

    private:
        template <typename T>
        static void Classify(std::vector<Detail::ComponentId>& archetypeIds,
                             std::vector<Detail::ComponentId>& sparseIds)
        {
            if constexpr (IsSparse<T>)
            {
                sparseIds.push_back(Detail::ComponentIdOf<T>());
            }
            else
            {
                archetypeIds.push_back(Detail::ComponentIdOf<T>());
            }
        }

        template <typename T>
        void AddEnableable()
        {
            if constexpr (IsEnableable<T>)
            {
                m_EnableableRequired.push_back(Detail::ComponentIdOf<T>());
            }
        }

        Detail::NativeStorage* m_Storage;
        std::array<Detail::ComponentId, GetCount == 0 ? 1 : GetCount> m_GetIds;
        std::vector<Detail::Archetype*> m_Archetypes;
        std::vector<Detail::ComponentId> m_SparseRequired;
        std::vector<Detail::ComponentId> m_SparseExcluded;
        std::vector<Detail::ComponentId> m_EnableableRequired;
        bool m_PerRowChecks = false;
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

            EntityStorage& storage = m_Core.Storage();
            if (!storage.IsValid(entity))
            {
                return nullptr;
            }
            return storage.template TryGet<std::remove_const_t<T>>(entity);
        }

        /** True if the entity is alive. */
        bool IsAlive(const Entity entity) const { return m_Core.Storage().IsValid(entity); }

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
            assert(singleton != nullptr && "Singleton: nobody made it. Its writer makes it in OnCreate");
            return *singleton;
        }

        /** Pushes requests into the queue Q. Needs Append<Q>. Fixed phases only. */
        template <typename Q>
        QueueWriter<Q> Queue()
        {
            static_assert(IsFixed(Derived::Info.Phase),
                          "Only a fixed phase system appends to a queue. A frame rate system cannot change the "
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

            EntityStorage& storage = m_Core.Storage();
            const Entity e = storage.Create();
            (storage.template Add<std::decay_t<Cs>>(e, std::forward<Cs>(components)), ...);
            return e;
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
        void Add(const Entity entity, T value)
        {
            static_assert(Allows<A, AccessMode::Write, T>, "Add needs Write<T> in the access list.");
            Commands().template AddComponent<T>(entity, std::move(value));
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
            return QueryRange<Gets, TypeList<Ws...>, TypeList<Xs...>>(m_Core.Storage().Native());
        }

        template <typename Q>
        Q& QueueComponent()
        {
            Q* queue = m_Core.Storage().template TryGetSingleton<Q>();
            assert(queue != nullptr && "Queue: nobody made it. Its consumer makes it in OnCreate");
            return *queue;
        }

        SystemContextBase& m_Core;
    };

    namespace Detail
    {
        template <typename E>
        void PrepareOne()
        {
            if constexpr (std::is_base_of_v<AccessItemBase, E>)
            {
                // Give the type its ComponentId before the first tick, not in the middle of one
                (void)ComponentIdOf<typename E::Type>();
            }
        }

        template <typename... Es>
        void PrepareAll(TypeList<Es...>)
        {
            (PrepareOne<Es>(), ...);
        }

        /** Registers each component type in A with the storage. */
        template <typename A>
        void PrepareStorage(SystemSetupBase& setup)
        {
            (void)setup;
            PrepareAll(typename A::Items{});
        }
    } // namespace Detail
} // namespace Fling
