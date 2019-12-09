#if WITH_LUA
#pragma once
#include "pch.h"
#include "File.h"
#include "LuaManager.h"

namespace Fling
{
	/**
	* @brief Component that represents a lua script attached to an entity
	*/
	class ScriptComponent
	{
	public:

		/**
		* @brief Default constructor
		*/
		ScriptComponent();

		/**
		* @brief Parameterized constructor
		*
		* @param t_FilePath		The path to the lua script file
		* @param t_Ent			The entity this component is attached to
		*/
		ScriptComponent(const std::string t_FilePath, entt::entity t_Ent);

		/**
		* @brief Destructor
		*/
		~ScriptComponent();

	private:

		//The GUID representation of the script file
		Guid m_ScriptGUID;

		//A reference to the lua manager
		LuaManager* m_luaManager = nullptr;
	};
}
#endif