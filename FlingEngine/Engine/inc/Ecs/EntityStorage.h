#pragma once

#include "Ecs/Component.h"
#include "Ecs/detail/EnttBridge.h"
#include "Ecs/Entity.h"
#include "Ecs/TypeHash.h"
#include "Signals.h"

#include <cstddef>
#include <iterator>
#include <memory>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <unordered_map>
#include <utility>

namespace Fling
{
    class EntityStorage;

    /** The callback type for component hooks: the storage and the entity that changed. */
    using EntityHookSignature = void(EntityStorage&, Entity);

    /**
     * Every entity that has all of the Ts. Made by EntityStorage::View.
     *
     * Iterate it with a range-for (each element is an Entity), or call Each. Empty
     * types (tags) filter the entities but are not passed to the Each callback.
     */
    template <typename... Ts>
    class EntityView
    {
    public:
        explicit EntityView(entt::registry& registry) : m_View(registry.view<Ts...>()) {}

        class Iterator
        {
        public:
            using Inner = decltype(std::declval<entt::basic_view<entt::entity, Ts...>&>().begin());
            using iterator_category = std::forward_iterator_tag;
            using value_type = Entity;
            using difference_type = std::ptrdiff_t;
            using pointer = void;
            using reference = Entity;

            explicit Iterator(Inner inner) : m_Inner(inner) {}

            Entity operator*() const { return Detail::FromEntt(*m_Inner); }

            Iterator& operator++()
            {
                ++m_Inner;
                return *this;
            }

            bool operator==(const Iterator& other) const { return m_Inner == other.m_Inner; }

            bool operator!=(const Iterator& other) const { return m_Inner != other.m_Inner; }

        private:
            Inner m_Inner;
        };

        Iterator begin() const { return Iterator{m_View.begin()}; }

        Iterator end() const { return Iterator{m_View.end()}; }

        bool Empty() const { return begin() == end(); }

        /** The number of entities in the view. It walks the view. */
        size_t Size() const
        {
            size_t count = 0;
            for (auto it = begin(); it != end(); ++it)
            {
                ++count;
            }
            return count;
        }

        /** True if the entity has every T. */
        bool Contains(const Entity entity) const { return m_View.contains(Detail::ToEntt(entity)); }

        /** A component of an entity in the view. */
        template <typename T>
        T& Get(const Entity entity) const
        {
            return GetFromView<T>(Detail::ToEntt(entity));
        }

        /**
         * Call fn for each entity. fn takes (Entity, T&...) or (T&...), where the Ts
         * are the view's types that are not empty, in order.
         */
        template <typename Fn>
        void Each(Fn&& fn) const
        {
            for (const entt::entity e : m_View)
            {
                CallWithData(fn, e, DataTypes{});
            }
        }

    private:
        template <typename T>
        using KeepIfData = std::conditional_t<std::is_empty_v<T>, std::tuple<>, std::tuple<T>>;

        using DataTypes = decltype(std::tuple_cat(std::declval<KeepIfData<Ts>>()...));

        /** A view of one type has a get with no template argument. */
        template <typename T>
        T& GetFromView(const entt::entity e) const
        {
            if constexpr (sizeof...(Ts) == 1)
            {
                return m_View.get(e);
            }
            else
            {
                return m_View.template get<T>(e);
            }
        }

        template <typename Fn, typename... Ds>
        void CallWithData(Fn& fn, const entt::entity e, std::tuple<Ds...>) const
        {
            if constexpr (std::is_invocable_v<Fn&, Entity, Ds&...>)
            {
                fn(Detail::FromEntt(e), GetFromView<Ds>(e)...);
            }
            else
            {
                fn(GetFromView<Ds>(e)...);
            }
        }

        entt::basic_view<entt::entity, Ts...> m_View;
    };

    /**
     * The entities and components of one world, and the world's singletons.
     *
     * This is the one API that engine code uses to read and change entities. The
     * storage behind it is EnTT for now. The native backend replaces it without
     * changes to callers.
     *
     * Systems do not use this class. They use their typed context, which the
     * compiler checks against the system's access list.
     */
    class EntityStorage
    {
    public:
        EntityStorage() = default;
        EntityStorage(const EntityStorage&) = delete;
        EntityStorage& operator=(const EntityStorage&) = delete;
        ~EntityStorage() = default;

        // Entities -------------------------------------------------------------------

        Entity Create() { return Detail::FromEntt(m_Registry.create()); }

        /** Destroy the entity and all of its components. A stale or null handle is ignored. */
        void Destroy(const Entity entity)
        {
            if (IsValid(entity))
            {
                m_Registry.destroy(Detail::ToEntt(entity));
            }
        }

        /** True if the handle points at a living entity. */
        bool IsValid(const Entity entity) const
        {
            const entt::entity e = Detail::ToEntt(entity);
            return e != entt::null && m_Registry.valid(e);
        }

        /** Destroy every entity. Singletons stay. */
        void Clear() { m_Registry.reset(); }

        /** The number of living entities. */
        size_t EntityCount() const { return m_Registry.alive(); }

        /** Call fn(Entity) for each living entity. */
        template <typename Fn>
        void EachEntity(Fn&& fn) const
        {
            m_Registry.each([&](const entt::entity e) { fn(Detail::FromEntt(e)); });
        }

        // Components ------------------------------------------------------------------

        /** Add a T to the entity. The entity must not have one. */
        template <typename T, typename... Args>
        decltype(auto) Add(const Entity entity, Args&&... args)
        {
            return m_Registry.assign<T>(Detail::ToEntt(entity), std::forward<Args>(args)...);
        }

        /** Add a T to the entity, or replace the one it has. */
        template <typename T, typename... Args>
        decltype(auto) AddOrReplace(const Entity entity, Args&&... args)
        {
            return m_Registry.assign_or_replace<T>(Detail::ToEntt(entity), std::forward<Args>(args)...);
        }

        /** Replace the entity's T. The entity must have one. */
        template <typename T, typename... Args>
        decltype(auto) Replace(const Entity entity, Args&&... args)
        {
            return m_Registry.replace<T>(Detail::ToEntt(entity), std::forward<Args>(args)...);
        }

        /** The entity's T. Adds a default T first if the entity has none. */
        template <typename T>
        T& GetOrAdd(const Entity entity)
        {
            return m_Registry.get_or_assign<T>(Detail::ToEntt(entity));
        }

        /** Remove the entity's T. The entity must have one. */
        template <typename T>
        void Remove(const Entity entity)
        {
            m_Registry.remove<T>(Detail::ToEntt(entity));
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
            return m_Registry.has<Ts...>(Detail::ToEntt(entity));
        }

        /** The entity's T. The entity must have one. */
        template <typename T>
        T& Get(const Entity entity)
        {
            return m_Registry.get<T>(Detail::ToEntt(entity));
        }

        template <typename T>
        const T& Get(const Entity entity) const
        {
            return m_Registry.get<T>(Detail::ToEntt(entity));
        }

        /** The entity's T, or null if it has none. */
        template <typename T>
        T* TryGet(const Entity entity)
        {
            return m_Registry.try_get<T>(Detail::ToEntt(entity));
        }

        template <typename T>
        const T* TryGet(const Entity entity) const
        {
            return m_Registry.try_get<T>(Detail::ToEntt(entity));
        }

        /** The number of entities with a T. */
        template <typename T>
        size_t Count() const
        {
            return m_Registry.size<T>();
        }

        /** Every entity that has all of the Ts. */
        template <typename... Ts>
        EntityView<Ts...> View()
        {
            return EntityView<Ts...>{m_Registry};
        }

        // Singletons ------------------------------------------------------------------

        /** Make the world's T, replacing any that exists. */
        template <typename T, typename... Args>
        T& SetSingleton(Args&&... args)
        {
            return m_Registry.set<T>(std::forward<Args>(args)...);
        }

        /** The world's T. It must exist. */
        template <typename T>
        T& Singleton()
        {
            return m_Registry.ctx<T>();
        }

        template <typename T>
        const T& Singleton() const
        {
            return m_Registry.ctx<T>();
        }

        /** The world's T, or null if there is none. */
        template <typename T>
        T* TryGetSingleton()
        {
            return m_Registry.try_ctx<T>();
        }

        template <typename T>
        const T* TryGetSingleton() const
        {
            return m_Registry.try_ctx<T>();
        }

        /** Remove the world's T, if there is one. */
        template <typename T>
        void RemoveSingleton()
        {
            m_Registry.unset<T>();
        }

        // Hooks -----------------------------------------------------------------------

        /** Runs after a T is added to an entity. */
        template <typename T>
        Sink<EntityHookSignature> OnAdd()
        {
            return Sink<EntityHookSignature>{HooksFor<T>().Added};
        }

        /** Runs after an entity's T is replaced. */
        template <typename T>
        Sink<EntityHookSignature> OnReplace()
        {
            return Sink<EntityHookSignature>{HooksFor<T>().Replaced};
        }

        /** Runs before a T is removed from an entity, and before an entity with a T is destroyed. */
        template <typename T>
        Sink<EntityHookSignature> OnRemove()
        {
            return Sink<EntityHookSignature>{HooksFor<T>().Removed};
        }

        // Backend ---------------------------------------------------------------------

        /** The EnTT registry behind this storage. Only Ecs/ code uses it. It goes away with EnTT. */
        entt::registry& Native() { return m_Registry; }

        const entt::registry& Native() const { return m_Registry; }

    private:
        struct HookSet
        {
            EntityStorage* Owner = nullptr;
            Signal<EntityHookSignature> Added;
            Signal<EntityHookSignature> Replaced;
            Signal<EntityHookSignature> Removed;
        };

        template <typename T>
        struct TypedHooks : HookSet
        {
            void OnConstruct(const entt::entity e, entt::registry&, T&) { Added.Publish(*Owner, Detail::FromEntt(e)); }

            void OnUpdate(const entt::entity e, entt::registry&, T&) { Replaced.Publish(*Owner, Detail::FromEntt(e)); }

            void OnDestroy(const entt::entity e, entt::registry&) { Removed.Publish(*Owner, Detail::FromEntt(e)); }
        };

        template <typename T>
        HookSet& HooksFor()
        {
            static_assert(!std::is_empty_v<T>, "Hooks on tags are not supported");
            const std::type_index key{typeid(T)};
            auto found = m_Hooks.find(key);
            if (found != m_Hooks.end())
            {
                return *found->second;
            }
            auto hooks = std::make_unique<TypedHooks<T>>();
            hooks->Owner = this;
            m_Registry.on_construct<T>().template connect<&TypedHooks<T>::OnConstruct>(*hooks);
            m_Registry.on_replace<T>().template connect<&TypedHooks<T>::OnUpdate>(*hooks);
            m_Registry.on_destroy<T>().template connect<&TypedHooks<T>::OnDestroy>(*hooks);
            HookSet& result = *hooks;
            m_Hooks.emplace(key, std::move(hooks));
            return result;
        }

        entt::registry m_Registry;

        /** Destroyed before the registry. Destroying a registry calls no hooks. */
        std::unordered_map<std::type_index, std::unique_ptr<HookSet>> m_Hooks;
    };
} // namespace Fling
