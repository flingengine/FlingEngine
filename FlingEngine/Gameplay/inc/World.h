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
		
		/**
		 * @brief Called just before destruction.
		 */
        void Shutdown();

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

		/**
		 * @brief 	Based on all current entities in the registry serialize that data to a JSON file
		 * 			This will write out some core engine components along with the specified custom 
		 * 			game components.
		 * 
		 * @tparam ARGS Any component types from your game that need to be serialized 
		 * @param t_LevelToLoad File path to load (relative to the assets directory)
		 * @return True on success
		 */
		template<class ...ARGS>
		bool OutputLevelFile(const std::string& t_LevelToLoad);

		/**
		 * @brief 	Reset the current registry and load in new entities/components from a JSON file
		 * 			This will read in some core engine components along with the specified custom 
		 * 			game components.
		 * 
		 * @tparam ARGS Any component types from your game that need to be serialized 
		 * @param t_LevelToLoad File path to load (relative to the assets directory)
		 * @return True on success
		 */
		template<class ...ARGS>
		bool LoadLevelFile(const std::string& t_LevelToLoad);

		FORCEINLINE entt::registry& GetRegistry() const { return m_Registry; }

    private:
		
		void WriteLevel();

		/** The registry and represents all active entities in this world */
		entt::registry& m_Registry;

		/** The game will allow users to specify their own update/read/write functions */
		Fling::Game* m_Game = nullptr;

		/** Flag if the world should quit or not! */
		UINT8 m_ShouldQuit = false;
    };

} // namespace Fling

#include "World.inl"