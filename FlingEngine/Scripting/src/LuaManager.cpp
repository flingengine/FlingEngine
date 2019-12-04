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

	void LuaManager::RegisterScript(entt::hashed_string t_FileGUID, entt::entity t_Ent)
	{
		File* f = new File(t_FileGUID);
		LuaBehaviors luaBehavior;

		LoadScript(f, t_Ent, &luaBehavior);

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

	void LuaManager::Tick(float t_deltaTime)
	{
		for (auto const& script : m_LuaComponents)
		{
			if (script.second.Tick != sol::nil)
			{
				script.second.Tick(t_deltaTime);
			}
		}
	}

	void LuaManager::LoadScript(File* t_File, entt::entity t_Ent, LuaBehaviors* t_Behavior)
	{
		t_Behavior->LuaState.open_libraries(sol::lib::base);

		DefineLuaTypes(t_Behavior->LuaState);
		DefineLuaFunctions(t_Behavior->LuaState);

		t_Behavior->LuaState["entityTransform"] = m_Registry->get<Transform>(t_Ent);
		t_Behavior->LuaState["entity"] = t_Ent;

		t_Behavior->LuaState.script_file(t_File->GetFilepathReleativeToAssets());

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

	void LuaManager::SetTransform(entt::entity t_Ent, Transform t_Transform)
	{
		Transform& t0 = m_Registry->get<Transform>(t_Ent);
		t0.SetPos(t_Transform.GetPos());
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
		t_LuaState.new_usertype<entt::entity>("Entity");
	}

	void LuaManager::DefineLuaFunctions(sol::state& t_LuaState)
	{
		t_LuaState.set_function("Print", &LuaManager::LuaPrint, this);
		t_LuaState.set_function("SetTransform", &LuaManager::SetTransform, this);
	}
}
