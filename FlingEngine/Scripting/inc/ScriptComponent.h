#if WITH_LUA

#pragma once
#include "File.h"
#include "Serilization.h"

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
		
		/** Default ctor for serialization */
		ScriptComponent() = default;

		/**
		* @brief Destructor
		*/
		~ScriptComponent() = default;

		/**
		* @brief Getter for the lua script file reference
		*
		* @return Fling::File*
		*/
		inline File* GetScriptFile() { return m_ScriptFile; }

		template<class Archive>
		void save(Archive& t_Archive) const;

		template<class Archive>
		void load(Archive& t_Archive);

	private:

		//A reference to the script file
		File* m_ScriptFile = nullptr;
	};

	/** Serialization to an Archive */
	template<class Archive>
	inline void ScriptComponent::save(Archive& t_Archive) const
	{
		std::string ScriptPath = "INVALID_LUA_PATH";

		if (m_ScriptFile)
		{
			ScriptPath = m_ScriptFile->GetGuidString();
		}

		t_Archive(
			cereal::make_nvp("SCRIPT_PATH", ScriptPath)
		);
	}

	template<class Archive>
	inline void ScriptComponent::load(Archive& t_Archive)
	{
		std::string ScriptPath = "";

		t_Archive(
			cereal::make_nvp("SCRIPT_PATH", ScriptPath)
		);

		m_ScriptFile = File::Create(HS(ScriptPath.c_str())).get();
	}
}

#endif	// WITH_LUA