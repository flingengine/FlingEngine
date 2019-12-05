#pragma once

#include "Platform.h"
#include "Singleton.hpp"
#include "File.h"
#include <sol/sol.hpp>
#include "Components/Transform.h"
#include <entt/entity/registry.hpp>

namespace Fling
{
	/**
	* @brief Container that stores a LuaState and function callbacks for specified lua behaviors
	*/
	struct LuaBehaviors
	{
		sol::state LuaState;
		sol::function Tick = sol::nil;
		sol::function Start = sol::nil;
	};

	/**
	* @brief Singleton that manages all of the 
	*        lua components attached to entities
	*/
	class LuaManager : public Singleton<LuaManager>
	{
	public:

		/**
		* @brief Initialize the LuaManager
		*
		* @param t_Registry		A reference to the registry
		*/
		void Init(entt::registry* t_Registry);

		/**
		* @brief Shutdown the LuaManager
		*/
		void Shutdown() override;

		/**
		* @brief Register the specified script and entity with the LuaManager
		*
		* @param t_FileGUID		The GUID for the lua script file we are using
		* @param t_Ent			The entity this script is attached to
		*/
		void RegisterScript(Guid t_FileGUID, entt::entity t_Ent);

		/**
		* @brief Run the start method for all of the lua scripts
		*/
		void Start();

		/**
		* @brief Run the tick function for all of the lua scripts.
		*        This is called every frame.
		* @param t_deltaTime	The time between frames
		*/
		void Tick(float t_deltaTime);

	private:

		/**
		* @brief Load in the specified lua script for the entity
		*
		* @param t_File			The file for the lua script we are loading
		* @param t_Ent			The entity this component is attached to
		* @param t_Behavior		A pointer to the LuaBehaviors struct to store the state in
		*/
		void LoadScript(File* t_File, entt::entity t_Ent, LuaBehaviors* t_Behavior);

		/**
		* @brief Create a callback for the specified lua function and store it in the specified location
		*
		* @param t_LuaState			The LuaState to add the callback to
		* @param t_FunctionName		The name of the lua function to call
		* @param t_Callbacks		A pointer to the location where the callback will be stored
		*/
		void AddCallback(const sol::state& t_LuaState, const char* t_FunctionName, sol::function* t_Callbacks);

		/**
		* @brief Prints the specified string to the console
		*
		* @param t_Message	The string to print
		*/
		void LuaPrint(const std::string& t_Message);

		/**
		* @brief Helper method for defining custom types used in the lua
		*
		* @param t_LuaState		The LuaState to define the types in
		*/
		void DefineLuaTypes(sol::state& t_LuaState);

		/**
		* @brief Helper method for defining standard functions that can be called from the lua
		*
		* @param t_LuaState		The LuaState to define the functions in
		*/
		void DefineLuaFunctions(sol::state& t_LuaState);

		//A reference to the registry
		entt::registry* m_Registry = nullptr;

		//A map of all of the registered lua behaviors
		std::unordered_map<File*, LuaBehaviors> m_LuaComponents;
	};
}