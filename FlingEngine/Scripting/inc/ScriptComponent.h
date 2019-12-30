#if WITH_LUA
#pragma once
#include "pch.h"
#include "File.h"

namespace Fling
{
	/**
	* @brief Component that represents a lua script attached to an entity
	*/
	class ScriptComponent
	{
	public:

		/**
		* @brief Parameterized constructor
		*
		* @param t_FilePath		The path to the lua script file
		*/
		ScriptComponent(const std::string t_FilePath);

		/**
		* @brief Destructor
		*/
		~ScriptComponent();

		/**
		* @brief Getter for the lua script file reference
		*
		* @return Fling::File*
		*/
		inline File* GetScriptFile() { return m_ScriptFile; }

	private:

		//A reference to the script file
		File* m_ScriptFile = nullptr;
	};
}
#endif