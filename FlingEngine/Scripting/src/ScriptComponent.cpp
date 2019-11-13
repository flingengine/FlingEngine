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
	}

	ScriptComponent::~ScriptComponent()
	{
	}
}
