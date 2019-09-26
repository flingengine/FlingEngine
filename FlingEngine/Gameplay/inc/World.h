#pragma once

#include "NonCopyable.hpp"
#include "Level.h"
#include "Game.h"
#include "FlingConfig.h"

#include <string>
#include <fstream>

#include <entt/entity/registry.hpp>
#include <cereal/archives/json.hpp>

namespace Fling
{
	/**
	* The world holds all active levels in the game. There will only ever be exactly one World
	* instance at any given time. 
	* @see Level
	*/
    class World : public NonCopyable
    {
    public: 

		explicit World(entt::registry& t_Reg, Fling::Game* t_Game);

		/**
		* @brief	Initializes the world. Loads the StartLevel that is specified in the config.  
		* @note		Keep explicit Init and Shutdown functions to make the startup order more readable
		*/
        void Init();

        void Shutdown();

		/**
		* Called before the first Update tick on the world. 
		*/
		void PreTick();

        /**
         * @brief   Tick all active levels in the world
         * 
         * @param t_DeltaTime   Time between previous frame and the current one. 
         */
        void Update(float t_DeltaTime);

		/**
		 * @brief Check if the world wants to exit the program. 
		 * @see Engine::Tick 
		 * 
		 * @return True if the world has signaled for exit
		 */
		FORCEINLINE bool ShouldQuit() const { return m_ShouldQuit; }

		template<class ...ARGS>
		void OutputLevelFile();

		template<class ...ARGS>
		void LoadLevelFile();

    private:
		
		void WriteLevel();

		/** The registry and represents all active entities in this world */
		entt::registry& m_Registry;

		/** The game will allow users to specify their own update/read/write functions */
		Fling::Game* m_Game = nullptr;

		/** Flag if the world should quit or not! */
		UINT8 m_ShouldQuit = false;
    };

	template<class ...ARGS>
	void World::OutputLevelFile()
	{
		std::string LevelToLoad = FlingPaths::EngineAssetsDir() + "/" + FlingConfig::GetString("Game", "StartLevel");

		std::ofstream OutStream(LevelToLoad);
		if(!OutStream.is_open())
		{
			OutStream.close();
			F_LOG_ERROR("Failed to open out stream to level {}", LevelToLoad);
			return;
		}
		
		F_LOG_TRACE("Outputting Level file to {}", LevelToLoad);

    	cereal::JSONOutputArchive archive(OutStream);

		// Write out a copy of what is in this registry 
		m_Registry.snapshot()
			.entities(archive)
			.component<ARGS...>(archive);

		//m_Registry.reset();

		//OutStream.close();
	}
	
	template<class ...ARGS>
	void World::LoadLevelFile()
	{
		std::string LevelToLoad = FlingPaths::EngineAssetsDir() + "/" + FlingConfig::GetString("Game", "StartLevel");
		F_LOG_TRACE("Load Scene file to: {}", LevelToLoad);

		// Create a cereal input stream
		std::ifstream InputStream(LevelToLoad);
		if(!InputStream.is_open())
		{
			InputStream.close();
			F_LOG_ERROR("Failed to open input stream from file: {}", LevelToLoad);
			return;
		}

		F_LOG_TRACE("Loading Level file from {}", LevelToLoad);

    	cereal::JSONInputArchive archive(InputStream);

		// This type of loader requires the registry to be cleared first
		m_Registry.reset();

		// Load into thre registry based on the serialization from the stream
		m_Registry.loader()
			.entities(archive)
			.component<ARGS...>(archive);
		
		//InputStream.close();
	}

} // namespace Fling