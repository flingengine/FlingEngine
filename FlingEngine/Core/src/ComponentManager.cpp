#include "pch.h"
#include "ComponentManager.h"

namespace Fling
{
    void ComponentManager::Init()
    {
        // Create registries
        F_LOG_TRACE("Component Man Init!");
        CreateRegistry();
    }

    void ComponentManager::Shutdown()
    {
        // Destroy all components in the registries
        F_LOG_TRACE("Component Man Shutdown!");
        if(m_Registry)
        {
            delete m_Registry;
            m_Registry = nullptr;
        }
    }

    entt::registry* ComponentManager::GetRegistry()
    {
        return ComponentManager::Get().GetRegistryRaw();
    }

    void ComponentManager::CreateRegistry()
    {
        F_LOG_TRACE("Create component registry!");
        m_Registry = new entt::registry();
    }

    entt::entity ComponentManager::CreateEntity()
    {
        if(!m_Registry)
        {
            CreateRegistry();
        }

        return m_Registry->create();
    }

    void ComponentManager::DestroyEntity(entt::entity& t_Ent)
    {
        if(m_Registry && m_Registry->valid(t_Ent))
        {
            m_Registry->destroy(t_Ent);
        }
    }
} // namespace Fling