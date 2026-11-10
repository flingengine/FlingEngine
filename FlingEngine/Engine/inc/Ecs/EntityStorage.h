#pragma once

#include "Ecs/Component.h"
#include "Ecs/detail/NativeStorage.h"
#include "Ecs/Entity.h"
#include "Ecs/Query.h"
#include "Ecs/QueryFilters.h"
#include "MathUtils.h"
#include "Signals.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <memory>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fling
{
    class EntityStorage;

    /** The callback type for component hooks: the storage and the entity that changed. */
    using EntityHookSignature = void(EntityStorage&, Entity);

    namespace Detail
    {
        /** A T from args, with () when T has a matching constructor and {} otherwise (aggregates). */
        template <typename T, typename... Args>
        T MakeValue(Args&&... args)
        {
            if constexpr (std::is_constructible_v<T, Args...>)
            {
                return T(std::forward<Args>(args)...);
            }
            else
            {
                return T{std::forward<Args>(args)...};
            }
        }

        /** Construct a T at memory from args, in place: the value MakeValue returns is made there, with no move. */
        template <typename T, typename... Args>
        T* ConstructAt(void* memory, Args&&... args)
        {
            return ::new (memory) T(MakeValue<T>(std::forward<Args>(args)...));
        }
    } // namespace Detail

    /**
     * Every entity that has all of the Ts, found when the view is made. Made by
     * EntityStorage::View, for code outside systems (systems use ctx.Query). The
     * entities come from the same QueryRange that ctx.Query returns.
     *
     * The list of entities is taken when the view is made, so the loop body may add
     * and remove components. Iterate it with a range-for (each element is an Entity),
     * or call Each. Empty types (tags) filter the entities but are not passed to Each.
     */
    template <typename... Ts>
    class EntityView
    {
    public:
        EntityView(EntityStorage& storage, std::vector<Entity> entities)
            : m_Storage(&storage), m_Entities(std::move(entities))
        {
        }

        auto begin() const { return m_Entities.begin(); }

        auto end() const { return m_Entities.end(); }

        bool Empty() const { return m_Entities.empty(); }

        size_t Size() const { return m_Entities.size(); }

        /** True if the entity was in the view when it was made. */
        bool Contains(const Entity entity) const
        {
            return std::find(m_Entities.begin(), m_Entities.end(), entity) != m_Entities.end();
        }

        /** A component of an entity in the view. */
        template <typename T>
        T& Get(const Entity entity) const;

        /**
         * Call fn for each entity that still has every T. fn takes (Entity, T&...) or
         * (T&...), where the Ts are the view's types that are not empty, in order.
         */
        template <typename Fn>
        void Each(Fn&& fn) const;

    private:
        template <typename T>
        using KeepIfData = std::conditional_t<std::is_empty_v<T>, std::tuple<>, std::tuple<T>>;

        using DataTypes = decltype(std::tuple_cat(std::declval<KeepIfData<Ts>>()...));

        template <typename Fn, typename... Ds>
        void CallWithData(Fn& fn, Entity entity, std::tuple<Ds...>) const;

        EntityStorage* m_Storage;
        std::vector<Entity> m_Entities;
    };

    /**
     * The entities and components of one world, and the world's singletons.
     *
     * This is the one API that engine code uses to read and change entities. The
     * storage behind it is archetypes in chunks (Detail::NativeStorage). Adding or
     * removing a Table component moves the entity to another archetype, so a
     * reference to one of its components is not valid after that. Keep the Entity,
     * and Get the component again.
     *
     * Systems do not use this class. They use their typed context, which the
     * compiler checks against the system's access list.
     */
    class EntityStorage
    {
    public:
        EntityStorage() : m_Native(*this) {}
        EntityStorage(const EntityStorage&) = delete;
        EntityStorage& operator=(const EntityStorage&) = delete;
        ~EntityStorage() = default;

        // Entities -------------------------------------------------------------------

        Entity Create() { return m_Native.Create(); }

        /**
         * Make an entity with the given components. It goes straight to the archetype of
         * all of them, so this costs much less than Create and then Add for each. The
         * Added hooks run after every component is in place, in the order given. The call
         * must have every component that one of them requires (RequiredOf), or it does
         * not compile.
         */
        template <typename... Cs>
        requires(sizeof...(Cs) > 0) Entity Create(Cs&&... components)
        {
            static_assert(((Component<std::decay_t<Cs>> && !IsSingletonComponent<std::decay_t<Cs>>) && ...),
                          "Create takes Table, Tag, and Sparse components.");
            static_assert(AllUnique<std::decay_t<Cs>...>, "A component appears twice in one Create.");
            static_assert((HasRequired<std::decay_t<Cs>, std::decay_t<Cs>...> && ...),
                          "Create (or ctx.Spawn) makes an entity with a component, and without a component that it "
                          "requires. Look at the Requires list of each component in the call, and add what is "
                          "missing.");
            constexpr size_t Count = sizeof...(Cs);
            const std::array<Detail::ComponentId, Count> ids{Detail::ComponentIdOf<std::decay_t<Cs>>()...};
            std::array<void*, Count> memory{};
            const Entity entity = m_Native.CreateWith(ids, memory);
            size_t next = 0;
            (ConstructComponent<std::decay_t<Cs>>(memory[next++], std::forward<Cs>(components)), ...);
            for (const Detail::ComponentId id : ids)
            {
                // A hook may destroy the entity or remove a later component
                if (m_Native.HasHooks(id) && m_Native.Has(entity, id))
                {
                    m_Native.EndAdd(entity, id);
                }
            }
            return entity;
        }

        /** Destroy the entity and all of its components. A stale or null handle is ignored. */
        void Destroy(const Entity entity) { m_Native.Destroy(entity); }

        /** True if the handle points at a living entity. */
        bool IsValid(const Entity entity) const { return m_Native.IsValid(entity); }

        /** Destroy every entity. The Removed hooks run. Singletons stay. */
        void Clear() { m_Native.Clear(); }

        /**
         * Give the memory blocks that hold no rows back to the OS, and return the bytes
         * freed. Storage keeps its pages while a game runs, so call this after a big
         * change, such as when a level unloads.
         */
        size_t TrimMemory() { return m_Native.TrimMemory(); }

        /** The number of living entities. */
        size_t EntityCount() const { return m_Native.EntityCount(); }

        /** Call fn(Entity) for each living entity. The list is taken first, so fn may change the storage. */
        template <typename Fn>
        void EachEntity(Fn&& fn) const
        {
            for (const Entity entity : m_Native.AllEntities())
            {
                fn(entity);
            }
        }

        // Components ------------------------------------------------------------------

        /**
         * Add a T to the entity. The entity must not have one, and must have every component
         * that T requires (RequiredOf). Returns the new component (nothing for a tag). An Added hook must not remove
         * the T or destroy the entity, because the reference is to the component after the hooks ran.
         */
        template <typename T, typename... Args>
        decltype(auto) Add(const Entity entity, Args&&... args)
        {
            const Detail::ComponentId id = Detail::ComponentIdOf<T>();
            void* memory = m_Native.BeginAdd(entity, id);
            if constexpr (std::is_empty_v<T>)
            {
                // A Table tag has no memory. A Sparse tag has a value, which must be constructed.
                if (memory != nullptr)
                {
                    Detail::ConstructAt<T>(memory, std::forward<Args>(args)...);
                }
                m_Native.EndAdd(entity, id);
            }
            else
            {
                T* constructed = Detail::ConstructAt<T>(memory, std::forward<Args>(args)...);
                if (!m_Native.HasHooks(id))
                {
                    return *constructed;
                }
                m_Native.EndAdd(entity, id);
                // A hook may move the entity, so find the component again
                T* added = static_cast<T*>(m_Native.Get(entity, id));
                assert(added != nullptr && "An Added hook removed the component or destroyed the entity");
                return *added;
            }
        }

        /** Add a T to the entity, or replace the one it has. */
        template <typename T, typename... Args>
        decltype(auto) AddOrReplace(const Entity entity, Args&&... args)
        {
            if (Has<T>(entity))
            {
                return Replace<T>(entity, std::forward<Args>(args)...);
            }
            return Add<T>(entity, std::forward<Args>(args)...);
        }

        /** Replace the entity's T. The entity must have one. */
        template <typename T, typename... Args>
        decltype(auto) Replace(const Entity entity, Args&&... args)
        {
            const Detail::ComponentId id = Detail::ComponentIdOf<T>();
            if constexpr (std::is_empty_v<T>)
            {
                m_Native.NotifyReplaced(entity, id);
            }
            else
            {
                T& existing = *static_cast<T*>(m_Native.Get(entity, id));
                existing = Detail::MakeValue<T>(std::forward<Args>(args)...);
                m_Native.NotifyReplaced(entity, id);
                return *static_cast<T*>(m_Native.Get(entity, id));
            }
        }

        /** The entity's T. Adds a default T first if the entity has none. */
        template <typename T>
        T& GetOrAdd(const Entity entity)
        {
            if (T* existing = TryGet<T>(entity))
            {
                return *existing;
            }
            return Add<T>(entity);
        }

        /** Remove the entity's T. The entity must have one, and no component that requires T (RequiredOf). */
        template <typename T>
        void Remove(const Entity entity)
        {
            m_Native.Remove(entity, Detail::ComponentIdOf<T>());
        }

        /**
         * Add a copy of value to every entity that has the components of the With filters,
         * none of the Without filters, and no T:
         * `storage.AddToAll(Burning{}, Fling::With<Health>{}, Fling::Without<Wet>{})`.
         * Whole chunks move at once, so this costs much less than Add for each entity.
         * The filters take Table, Enableable (enabled or not), and Tag components. If T
         * is Sparse or has hooks, the entities are done one at a time. Returns how many
         * entities got a T. Do not call it while a query is being iterated.
         */
        template <typename T, typename... Filters>
        size_t AddToAll(const T& value, Filters...)
        {
            static_assert(Component<T> && !IsSingletonComponent<T>, "AddToAll adds a Table, Tag, or Sparse component.");
            static_assert(std::is_copy_constructible_v<T>, "AddToAll copies the value to each entity.");
            const BulkFilterIds<Filters...> filter;
            return m_Native.AddToAll(
                Detail::ComponentIdOf<T>(), &value, &FillCopies<T>, filter.Required, filter.Excluded);
        }

        /**
         * Remove T from every entity that has it, the components of the With filters, and
         * none of the Without filters. Whole chunks move at once. See AddToAll. Returns how
         * many entities lost a T.
         */
        template <typename T, typename... Filters>
        size_t RemoveFromAll(Filters...)
        {
            static_assert(Component<T> && !IsSingletonComponent<T>,
                          "RemoveFromAll removes a Table, Tag, or Sparse component.");
            const BulkFilterIds<Filters...> filter;
            return m_Native.RemoveFromAll(Detail::ComponentIdOf<T>(), filter.Required, filter.Excluded);
        }

        /** Remove the entity's T if it has one. */
        template <typename T>
        void RemoveIfPresent(const Entity entity)
        {
            if (Has<T>(entity))
            {
                Remove<T>(entity);
            }
        }

        /** True if the entity has every T. */
        template <typename... Ts>
        bool Has(const Entity entity) const
        {
            return (m_Native.Has(entity, Detail::ComponentIdOf<Ts>()) && ...);
        }

        /** The entity's T. The entity must have one. */
        template <typename T>
        T& Get(const Entity entity)
        {
            T* component = TryGet<T>(entity);
            assert(component != nullptr && "Get on an entity that does not have the component");
            return *component;
        }

        template <typename T>
        const T& Get(const Entity entity) const
        {
            const T* component = TryGet<T>(entity);
            assert(component != nullptr && "Get on an entity that does not have the component");
            return *component;
        }

        /** The entity's T, or null if it has none. */
        template <typename T>
        T* TryGet(const Entity entity)
        {
            return static_cast<T*>(m_Native.Get(entity, Detail::ComponentIdOf<T>()));
        }

        template <typename T>
        const T* TryGet(const Entity entity) const
        {
            return static_cast<const T*>(m_Native.Get(entity, Detail::ComponentIdOf<T>()));
        }

        /** The number of entities with a T. */
        template <typename T>
        size_t Count() const
        {
            return m_Native.Count(Detail::ComponentIdOf<T>());
        }

        /** Turn an Enableable component on or off. It stays on the entity, and the entity does not move. */
        template <typename T>
        void SetEnabled(const Entity entity, const bool enabled)
        {
            static_assert(ComponentInfoOf<T>.Storage == ComponentStorage::Enableable,
                          "SetEnabled needs a component with .Storage = ComponentStorage::Enableable.");
            m_Native.SetEnabled(entity, Detail::ComponentIdOf<T>(), enabled);
        }

        template <typename T>
        bool IsEnabled(const Entity entity) const
        {
            return m_Native.IsEnabled(entity, Detail::ComponentIdOf<T>());
        }

        /** Every entity that has all of the Ts (and, for Enableable ones, has them on). */
        template <typename... Ts>
        EntityView<Ts...> View()
        {
            return EntityView<Ts...>(*this, Gather<Ts...>());
        }

        // Singletons ------------------------------------------------------------------

        /** Make the world's T, replacing any that exists. */
        template <typename T, typename... Args>
        T& SetSingleton(Args&&... args)
        {
            // In the storage's own memory, not the heap
            const Detail::ComponentId id = Detail::ComponentIdOf<T>();
            const size_t bytes = Math::Max(sizeof(T), alignof(T));
            void* memory = m_Native.AllocateSingleton(id, sizeof(T), alignof(T));
            T* object = Detail::ConstructAt<T>(memory, std::forward<Args>(args)...);
            m_Native.SetSingleton(id, object, bytes, [](void* p) { static_cast<T*>(p)->~T(); });
            return *object;
        }

        /** The world's T. It must exist. */
        template <typename T>
        T& Singleton()
        {
            T* object = TryGetSingleton<T>();
            assert(object != nullptr && "Singleton: the world has none");
            return *object;
        }

        template <typename T>
        const T& Singleton() const
        {
            const T* object = TryGetSingleton<T>();
            assert(object != nullptr && "Singleton: the world has none");
            return *object;
        }

        /** The world's T, or null if there is none. */
        template <typename T>
        T* TryGetSingleton()
        {
            return static_cast<T*>(m_Native.GetSingleton(Detail::ComponentIdOf<T>()));
        }

        template <typename T>
        const T* TryGetSingleton() const
        {
            return static_cast<const T*>(m_Native.GetSingleton(Detail::ComponentIdOf<T>()));
        }

        /** Remove the world's T, if there is one. */
        template <typename T>
        void RemoveSingleton()
        {
            m_Native.RemoveSingleton(Detail::ComponentIdOf<T>());
        }

        // Hooks -----------------------------------------------------------------------

        /** Runs after a T is added to an entity. */
        template <typename T>
        Sink<EntityHookSignature> OnAdd()
        {
            return Sink<EntityHookSignature>{m_Native.Hooks(Detail::ComponentIdOf<T>()).Added};
        }

        /** Runs after an entity's T is replaced (AddOrReplace or Replace). */
        template <typename T>
        Sink<EntityHookSignature> OnReplace()
        {
            return Sink<EntityHookSignature>{m_Native.Hooks(Detail::ComponentIdOf<T>()).Replaced};
        }

        /** Runs before a T is removed from an entity, and before an entity with a T is destroyed. */
        template <typename T>
        Sink<EntityHookSignature> OnRemove()
        {
            return Sink<EntityHookSignature>{m_Native.Hooks(Detail::ComponentIdOf<T>()).Removed};
        }

        // Telemetry and backend ---------------------------------------------------------

        /** Archetype, chunk, and entity counts, and how many rows have moved. */
        EcsStats Stats() const { return m_Native.Stats(); }

        /** The storage behind this API. Only Ecs/ code uses it. */
        Detail::NativeStorage& Native() { return m_Native; }

        const Detail::NativeStorage& Native() const { return m_Native; }

    private:
        /** The sorted ids of filter types, which must be in the archetypes (Table, Enableable, or Tag). */
        template <typename... Ts>
        static std::array<Detail::ComponentId, sizeof...(Ts)> SortedFilterIds(TypeList<Ts...>)
        {
            static_assert(
                ((Detail::IsFilterComponent<Ts> && ComponentInfoOf<Ts>.Storage != ComponentStorage::Sparse) && ...),
                "AddToAll and RemoveFromAll filter by Table, Enableable, and Tag components only.");
            std::array<Detail::ComponentId, sizeof...(Ts)> ids{Detail::ComponentIdOf<Ts>()...};
            std::sort(ids.begin(), ids.end());
            return ids;
        }

        /** The sorted ids that the With and Without filters of AddToAll and RemoveFromAll name. */
        template <typename... Filters>
        struct BulkFilterIds
        {
            static_assert(Detail::AreFilters<Filters...>,
                          "AddToAll and RemoveFromAll take only Fling::With<...> and Fling::Without<...> filters.");

            std::array<Detail::ComponentId, Detail::WithTypes<Filters...>::Size> Required =
                SortedFilterIds(Detail::WithTypes<Filters...>{});
            std::array<Detail::ComponentId, Detail::WithoutTypes<Filters...>::Size> Excluded =
                SortedFilterIds(Detail::WithoutTypes<Filters...>{});
        };

        template <typename T, typename Arg>
        static void ConstructComponent(void* memory, Arg&& value)
        {
            // Null for a Table tag, which has no memory
            if (memory != nullptr)
            {
                Detail::ConstructAt<T>(memory, std::forward<Arg>(value));
            }
        }

        template <typename T>
        static void FillCopies(void* dst, const size_t count, const void* value)
        {
            std::uninitialized_fill_n(static_cast<T*>(dst), count, *static_cast<const T*>(value));
        }

        template <typename... Ts>
        std::vector<Entity> Gather();

        mutable Detail::NativeStorage m_Native;
    };

    template <typename... Ts>
    std::vector<Entity> EntityStorage::Gather()
    {
        // The same query that systems use, with every type as a filter
        std::vector<Entity> result;
        QueryRange<TypeList<>, TypeList<std::remove_const_t<Ts>...>, TypeList<>>(m_Native).Each(
            [&result](const Entity entity) { result.push_back(entity); });
        return result;
    }

    template <typename... Ts>
    template <typename T>
    T& EntityView<Ts...>::Get(const Entity entity) const
    {
        return m_Storage->template Get<T>(entity);
    }

    template <typename... Ts>
    template <typename Fn>
    void EntityView<Ts...>::Each(Fn&& fn) const
    {
        for (const Entity entity : m_Entities)
        {
            // A change made by an earlier call can remove a type from this entity
            if (m_Storage->IsValid(entity) && m_Storage->template Has<Ts...>(entity))
            {
                CallWithData(fn, entity, DataTypes{});
            }
        }
    }

    template <typename... Ts>
    template <typename Fn, typename... Ds>
    void EntityView<Ts...>::CallWithData(Fn& fn, const Entity entity, std::tuple<Ds...>) const
    {
        if constexpr (std::is_invocable_v<Fn&, Entity, Ds&...>)
        {
            fn(entity, m_Storage->template Get<Ds>(entity)...);
        }
        else
        {
            fn(m_Storage->template Get<Ds>(entity)...);
        }
    }
} // namespace Fling
