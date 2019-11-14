#include "LuaManager.h"
#include "pch.h"

namespace Fling
{
	void LuaManager::Init()
	{
	}

	void LuaManager::Shutdown()
	{
	}

	void LuaManager::RegisterScript(entt::hashed_string t_FileGUID)
	{
		File* f = new File(t_FileGUID);
		LuaBehaviors luaBehavior;

		LoadScript(f, &luaBehavior);

		m_LuaComponents[f] = std::move(luaBehavior);
	}

	void LuaManager::Start()
	{
		for (auto const& script : m_LuaComponents)
		{
			if (script.second.Start != sol::nil)
			{
				script.second.Start();
			}
		}
	}

	void LuaManager::LoadScript(File* t_File, LuaBehaviors* t_Behavior)
	{
		t_Behavior->LuaState.open_libraries(sol::lib::base);

		t_Behavior->LuaState.script_file(t_File->GetFilepathReleativeToAssets());

		t_Behavior->LuaState.set_function("Print", &LuaManager::TestCallback, this);
		AddCallback(t_Behavior->LuaState, "start", &t_Behavior->Start);
	}

	void LuaManager::AddCallback(const sol::state& t_LuaState, const char* t_FunctionName, sol::function* t_Callbacks)
	{
		*t_Callbacks = sol::nil;
		sol::optional<sol::function> function = t_LuaState[t_FunctionName];

		if (function != sol::nullopt)
		{
			*t_Callbacks = function.value();
		}
	}

	void LuaManager::TestCallback()
	{
		F_LOG_TRACE("Lua print called");
	}
}
