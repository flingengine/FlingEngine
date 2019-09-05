#pragma once

#include <entt/entity/registry.hpp>
#include "Singleton.hpp"

#include <memory>

namespace Fling
{
    /**
     * @brief The component manager gives access to the component registry, which can 
     * be used to create entities and add components to said entities
     */
    class ComponentManager : public Singleton<ComponentManager>
    {
    public:
        virtual void Init() override;

        virtual void Shutdown() override;

        /**
         * @brief Get the Registry object   Static wrapper around GetRegistryRaw
         * 
         * @return entt::registry* Pointer to the registry. 
         * @see ComponentManager::GetRegistryRaw
         */
        static entt::registry* GetRegistry();

        /**
         * @brief Get the raw pointer Registry to be able to add entities to it
         * 
         * @return std::unique_ptr<entt::registry> 
         */
        entt::registry* GetRegistryRaw() const { return m_Registry; }

        /**
         * @brief Create an Entity object using the component manager registry
         * 
         * @return entt::entity
         */
        entt::entity CreateEntity();

        /**
         * @brief Destroys the given entity and all its component from the registry if it exists.
         * 
         * @param t_Ent The entity to destroy
         */
        void DestroyEntity(entt::entity& t_Ent);

    private: 

        /** Registry used to create entities and component */
        entt::registry* m_Registry = nullptr;

        /**
         * @brief Creates and initalizes the component registry
         * 
         */
        void CreateRegistry();

    };
}   // namespace Fling