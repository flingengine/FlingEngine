#include "LuaManager.h"
#include "pch.h"

namespace Fling
{
	void LuaManager::Init(entt::registry* t_Registry)
	{
		m_Registry = t_Registry;
	}

	void LuaManager::Shutdown()
	{
	}

	void LuaManager::RegisterScript(Guid t_FileGUID, entt::entity t_Ent)
	{
		//Create a file ussing the GUID
		File* f = new File(t_FileGUID);
		LuaBehaviors luaBehavior;

		LoadScript(f, t_Ent, &luaBehavior);

		m_LuaComponents[f] = std::move(luaBehavior);
	}

	void LuaManager::Start()
	{
		//Loop through all of the lua components
		for (auto const& script : m_LuaComponents)
		{
			//If this component has a start function, call it
			if (script.second.Start != sol::nil)
			{
				script.second.Start();
			}
		}
	}

	void LuaManager::Tick(float t_deltaTime)
	{
		//Loop through all of the lua components
		for (auto const& script : m_LuaComponents)
		{
			//If this component has a tick function, call it
			if (script.second.Tick != sol::nil)
			{
				script.second.Tick(t_deltaTime);
			}
		}
	}

	void LuaManager::LoadScript(File* t_File, entt::entity t_Ent, LuaBehaviors* t_Behavior)
	{
		t_Behavior->LuaState.open_libraries(sol::lib::base);

		//Define the standard lua types and functions that we want all lua scripts to have
		DefineLuaTypes(t_Behavior->LuaState);
		DefineLuaFunctions(t_Behavior->LuaState);

		//Give the script a reference to the transform
		t_Behavior->LuaState["entityTransform"] = &m_Registry->get<Transform>(t_Ent);

		//Load in the file
		t_Behavior->LuaState.script_file(t_File->GetFilepathReleativeToAssets());

		//Hook up the start and tick callbacks
		AddCallback(t_Behavior->LuaState, "start", &t_Behavior->Start);
		AddCallback(t_Behavior->LuaState, "tick", &t_Behavior->Tick);
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

	void LuaManager::LuaPrint(const std::string &t_Message)
	{
		F_LOG_TRACE(t_Message);
	}

	void LuaManager::DefineLuaTypes(sol::state& t_LuaState)
	{
		t_LuaState.new_usertype<glm::vec3>("vec3",
				sol::constructors<glm::vec3(float x, float y, float z)>(),
				"x", &glm::vec3::x,
				"y", &glm::vec3::y,
				"z", &glm::vec3::z
			);

		t_LuaState.new_usertype<Transform>("Transform",
				"CalculateWorldMatrix", &Transform::CalculateWorldMatrix,
				"GetPos", &Transform::GetPos,
				"GetRotation", &Transform::GetRotation,
				"GetScale", &Transform::GetScale,
				"SetPos", &Transform::SetPos,
				"SetRotation", &Transform::SetRotation,
				"SetScale", &Transform::SetScale
				);
	}

	void LuaManager::DefineLuaFunctions(sol::state& t_LuaState)
	{
		t_LuaState.set_function("Print", &LuaManager::LuaPrint, this);
	}
}
