#pragma once

#include "Platform.h"
#include "Singleton.hpp"
#include "File.h"
#include <sol/sol.hpp>

namespace Fling
{
	struct LuaBehaviors
	{
		sol::state LuaState;
		sol::function Tick = sol::nil;
		sol::function Start = sol::nil;
	};

	class LuaManager : public Singleton<LuaManager>
	{
	public:
		void Init() override;

		void Shutdown() override;

		void RegisterScript(entt::hashed_string t_FileGUID);

		void Start();
	private:

		void LoadScript(File* t_File, LuaBehaviors* t_Behavior);
		void AddCallback(const sol::state& t_LuaState, const char* t_FunctionName, sol::function* t_Callbacks);
		void TestCallback();
		std::unordered_map<File*, LuaBehaviors> m_LuaComponents;
	};
}