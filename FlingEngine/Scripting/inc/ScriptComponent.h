#if WITH_LUA
#pragma once
#include "pch.h"
#include "File.h"
#include "LuaManager.h"

namespace Fling
{
	class ScriptComponent
	{
	public:
		ScriptComponent();
		ScriptComponent(const std::string filePath, entt::entity t_Ent);
		~ScriptComponent();
	private:
		Guid m_ScriptGUID;

		LuaManager* m_luaManager = nullptr;
	};
}
#endif