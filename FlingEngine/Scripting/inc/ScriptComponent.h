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
		ScriptComponent(const std::string filePath);
		~ScriptComponent();
	private:
		Guid m_ScriptGUID;

		LuaManager* m_luaManager = nullptr;
	};
}