#if WITH_LUA
#include "pch.h"
#include "ScriptComponent.h"

namespace Fling
{
	ScriptComponent::ScriptComponent(const std::string t_FilePath)
	{
		//Convert the filepath into a GUID
		m_ScriptFile = new File(entt::hashed_string{ t_FilePath.c_str() });
	}

	ScriptComponent::~ScriptComponent()
	{
	}
}
#endif
