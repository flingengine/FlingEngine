#if WITH_LUA
#include "pch.h"
#include "ScriptComponent.h"

namespace Fling
{
	ScriptComponent::ScriptComponent()
	{
		m_ScriptGUID = "";
	}

	ScriptComponent::ScriptComponent(const std::string t_FilePath, entt::entity t_Ent)
	{
		//Convert the filepath into a GUID
		m_ScriptGUID = entt::hashed_string{ t_FilePath.c_str() };

		//Grab a reference to the lua manager
		m_luaManager = &LuaManager::Get();

		//If the filepath isn't empty, register the script with the lua manager
		if (t_FilePath.length() > 0)
		{
			m_luaManager->RegisterScript(m_ScriptGUID, t_Ent);
		}
	}

	ScriptComponent::~ScriptComponent()
	{
	}
}
#endif
