#pragma once

#include "Platform.h"
#include "Singleton.hpp"
#include "ScriptComponent.h"
#include <sol/sol.hpp>

namespace Fling
{
	class LuaManager : public Singleton<LuaManager>
	{
	public:
		virtual void Init() override;

		virtual void Shutdown() override;

		void RegisterScript(const std::string& t_File);
	private:
		std::unordered_map<std::string, ScriptComponent> m_LuaComponents;
	};
}