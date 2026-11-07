#pragma once

#include "Ecs/Component.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/Reflection.h"
#include "Ecs/SystemRegistry.h"

#include <functional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace Fling
{
    /** A component type that was deleted. Load ignores it without a warning, or converts it. */
    struct RemovedComponentDesc
    {
        std::string Name;

        /** Optional: turn the old saved object into components on the entity. */
        std::function<void(EntityStorage&, Entity, const Json&)> Convert;
    };

    /**
     * What each module adds to a world: its component types and its systems. Each
     * module has one `void Register<Module>(ModuleRegistration&)` function, and that
     * function is the module's table of contents, with one line per feature.
     *
     * A world makes one ModuleRegistration. The application calls the register
     * function of each module that it links, then the game's Register. The world
     * then builds its TypeRegistry and its schedule from it. A test makes its own.
     */
    class ModuleRegistration
    {
    public:
        explicit ModuleRegistration(SystemRegistry& systems) : m_Systems(&systems) {}

        ModuleRegistration(const ModuleRegistration&) = delete;
        ModuleRegistration& operator=(const ModuleRegistration&) = delete;

        /**
         * Adds the component type T. The name, kind, and transient flag come from
         * T::Component. build(TypeBuilder<T>&) describes the fields. A second call
         * for the same name is ignored with a warning.
         */
        template <::Fling::Component T, typename Fn>
        void Component(Fn&& build)
        {
            static_assert(std::is_invocable_v<Fn&, TypeBuilder<T>&>, "build takes a TypeBuilder<T>&.");
            static_assert(IsWellFormedComponent<T>,
                          "The ComponentInfo needs a name, and Kind = Tag exactly when the struct is empty.");
            static_assert(!std::is_pointer_v<T>, "A component cannot be a pointer.");
            static_assert(IsSingletonComponent<T> || std::is_default_constructible_v<T>,
                          "A table component must be default constructible, so that load and the editor can add it.");
            TypeDesc desc = MakeDesc<T>();
            TypeBuilder<T> builder(desc);
            build(builder);
            AddType(std::move(desc));
        }

        /** Adds a component type with no reflected fields: a tag, or a transient type. */
        template <::Fling::Component T>
        void Component()
        {
            Component<T>([](TypeBuilder<T>&) {});
        }

        /** Adds one instance of the system T to the world. */
        template <typename T>
        void System()
        {
            m_Systems->Add<T>();
        }

        /** The systems, for phases and orders between modules. */
        SystemRegistry& Systems() { return *m_Systems; }

        /** A deleted component type: load ignores its name without a warning, or converts it. */
        void RemovedComponent(const char* name, std::function<void(EntityStorage&, Entity, const Json&)> convert = {})
        {
            m_Removed.push_back(RemovedComponentDesc{name, std::move(convert)});
        }

        const std::vector<TypeDesc>& Types() const { return m_Types; }

        const std::vector<RemovedComponentDesc>& RemovedComponents() const { return m_Removed; }

    private:
        template <typename T>
        static TypeDesc MakeDesc()
        {
            constexpr ComponentInfo info = ComponentInfoOf<T>;
            TypeDesc desc;
            desc.Name = info.Name;
            desc.Hash = TypeHashOf<T>;
            desc.Kind = info.Kind;
            desc.Domain = info.Domain;
            desc.Transient = info.Transient;
            desc.Size = std::is_empty_v<T> ? 0 : sizeof(T);
            desc.Align = alignof(T);

            if constexpr (IsSingletonComponent<T>)
            {
                desc.GetSingleton = [](EntityStorage& storage) -> void*
                {
                    return storage.TryGetSingleton<T>();
                };
                desc.GetSingletonConst = [](const EntityStorage& storage) -> const void*
                {
                    return storage.TryGetSingleton<T>();
                };
                if constexpr (std::is_default_constructible_v<T>)
                {
                    desc.EmplaceSingleton = [](EntityStorage& storage, const TypeDesc::FillFn fill)
                    {
                        T object{};
                        if (fill != nullptr)
                        {
                            (*fill)(&object);
                        }
                        storage.SetSingleton<T>(std::move(object));
                    };
                }
            }
            else
            {
                desc.Has = [](const EntityStorage& storage, const Entity e)
                {
                    return storage.Has<T>(e);
                };
                desc.Emplace = [](EntityStorage& storage, const Entity e, const TypeDesc::FillFn fill)
                {
                    if constexpr (std::is_empty_v<T>)
                    {
                        (void)fill;
                        if (!storage.Has<T>(e))
                        {
                            storage.Add<T>(e);
                        }
                    }
                    else
                    {
                        T object{};
                        if (fill != nullptr)
                        {
                            (*fill)(&object);
                        }
                        storage.AddOrReplace<T>(e, std::move(object));
                    }
                };
                desc.Remove = [](EntityStorage& storage, const Entity e)
                {
                    storage.RemoveIfPresent<T>(e);
                };
                if constexpr (!std::is_empty_v<T>)
                {
                    desc.Get = [](EntityStorage& storage, const Entity e) -> void*
                    {
                        return storage.TryGet<T>(e);
                    };
                    desc.GetConst = [](const EntityStorage& storage, const Entity e) -> const void*
                    {
                        return storage.TryGet<T>(e);
                    };
                }
            }
            return desc;
        }

        /** Adds the type unless the name is taken. Defined in Reflection.cpp. */
        void AddType(TypeDesc desc);

        SystemRegistry* m_Systems;
        std::vector<TypeDesc> m_Types;
        std::vector<RemovedComponentDesc> m_Removed;
    };
} // namespace Fling
