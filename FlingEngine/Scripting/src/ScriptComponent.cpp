#include "pch.h"
#include "ScriptComponent.h"

namespace Fling
{
	ScriptComponent::ScriptComponent()
	{
		m_ScriptGUID = "";
	}

	ScriptComponent::ScriptComponent(const std::string filePath)
	{
		m_ScriptGUID = entt::hashed_string{ filePath.c_str() };
		m_luaManager = &LuaManager::Get();
		if (filePath.length() > 0)
		{
			m_luaManager->RegisterScript(m_ScriptGUID);
		}
	}

	ScriptComponent::~ScriptComponent()
	{
	}
}
