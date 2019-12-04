#pragma once

#include "Platform.h"
#include "Singleton.hpp"
#include "File.h"
#include <sol/sol.hpp>
#include "Components/Transform.h"
#include <entt/entity/registry.hpp>

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
		friend class Engine;
	public:
		void Init() override;

		void Shutdown() override;

		void RegisterScript(entt::hashed_string t_FileGUID, entt::entity t_Ent);

		void Start();

		void Tick(float t_deltaTime);
	private:

		void LoadScript(File* t_File, entt::entity t_Ent, LuaBehaviors* t_Behavior);
		void AddCallback(const sol::state& t_LuaState, const char* t_FunctionName, sol::function* t_Callbacks);
		void LuaPrint(const std::string& t_Message);
		void SetTransform(entt::entity t_Ent, Transform t_Transform);
		void DefineLuaTypes(sol::state& t_LuaState);
		void DefineLuaFunctions(sol::state& t_LuaState);

		entt::registry* m_Registry = nullptr;

		std::unordered_map<File*, LuaBehaviors> m_LuaComponents;
	};
}