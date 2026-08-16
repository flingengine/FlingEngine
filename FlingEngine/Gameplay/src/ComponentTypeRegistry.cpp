#include "pch.h"
#include "ComponentTypeRegistry.h"
#include "Components/Transform.h"
#include "Components/Name.hpp"

namespace Fling
{
	ComponentTypeRegistry& ComponentTypeRegistry::Get()
	{
		static ComponentTypeRegistry instance;
		return instance;
	}

	bool ComponentTypeRegistry::CanRegister(const char* name)
	{
		if (!name || name[0] == '\0')
		{
			F_LOG_ERROR("ComponentTypeRegistry::Register called with an empty type name");
			return false;
		}

		if (m_Sealed)
		{
			F_LOG_ERROR("ComponentTypeRegistry::Register('{}') called after Seal()", name);
			return false;
		}

		if (m_NameToIndex.find(name) != m_NameToIndex.end())
		{
			F_LOG_WARN("Duplicate component type name '{}', ignoring", name);
			return false;
		}

		return true;
	}

	void ComponentTypeRegistry::AddType(const char* name, ComponentTypeInfo info)
	{
		m_NameStorage.emplace_back(name);
		info.name = m_NameStorage.back().c_str();
		m_NameToIndex.emplace(m_NameStorage.back(), m_Types.size());
		m_Types.push_back(std::move(info));
	}

	void ComponentTypeRegistry::AddRegistrar(IComponentRegistrar* registrar)
	{
		if (m_Sealed)
		{
			F_LOG_ERROR("ComponentTypeRegistry::AddRegistrar called after Seal()");
			return;
		}

		if (registrar)
		{
			m_Registrars.push_back(registrar);
		}
	}

	void ComponentTypeRegistry::SetEditorWidget(const char* name, void(*fn)(entt::registry&, entt::entity))
	{
		const auto it = m_NameToIndex.find(name);
		if (it == m_NameToIndex.end())
		{
			F_LOG_WARN("ComponentTypeRegistry::SetEditorWidget('{}') called before the type was registered", name);
			return;
		}

		m_Types[it->second].drawEditorWidget = fn;
	}

	void ComponentTypeRegistry::RunExternalRegistrars()
	{
		if (m_Sealed)
		{
			F_LOG_ERROR("ComponentTypeRegistry::RunExternalRegistrars called after Seal()");
			return;
		}

		for (IComponentRegistrar* registrar : m_Registrars)
		{
			if (registrar)
			{
				registrar->RegisterComponents();
			}
		}
	}

	void ComponentTypeRegistry::Seal()
	{
		m_Sealed = true;
	}

	const ComponentTypeInfo* ComponentTypeRegistry::Find(const std::string& name) const
	{
		const auto it = m_NameToIndex.find(name);
		if (it == m_NameToIndex.end())
		{
			return nullptr;
		}
		return &m_Types[it->second];
	}

	void ComponentTypeRegistry::ResetForTests()
	{
		m_Types.clear();
		m_NameStorage.clear();
		m_NameToIndex.clear();
		m_Registrars.clear();
		m_Sealed = false;
	}

	void RegisterGameplayComponents()
	{
		ComponentTypeRegistry& registry = ComponentTypeRegistry::Get();
		registry.Register<Transform>("Transform");
		registry.Register<NameComponent>("NameComponent");
	}
}	// namespace Fling
