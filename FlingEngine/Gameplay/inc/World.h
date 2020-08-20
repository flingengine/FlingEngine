#pragma once

#include "NonCopyable.hpp"
#include "Level.h"
#include "Game.h"
#include "FlingConfig.h"

#include <string>
#include <fstream>

#include <entt/entity/registry.hpp>
#include "Serilization.h"

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
		 * @brief   Tick all active levels in the world and upates any Lua scripts that have Update functions
		 *
		 * @param t_DeltaTime   Time between previous frame and the current one.
		 */
		void Update(float t_DeltaTime);

		/**
		 * @brief Called just before destruction.
		 */
        void Shutdown();

		/** If there is a valid game state than this world will start it */
		void RequestGameStart();

		/** Attempt to stop the game if it is running */
		void RequestGameStop();

		/**
		 * @brief Check if the world wants to exit the program. 
		 * @see Engine::Tick 
		 * 
		 * @return True if the world has signaled for exit
		 */
		FORCEINLINE bool ShouldQuit() const { assert(m_Game); return m_ShouldQuit || m_Game->WantsToQuit(); }

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

		// The current state of the game, is it playing, stopped, paused, etc
		enum class WorldState : uint8
		{
			NONE,			// The game has not been initalized yet. Starting state.
			Initalized,		// True after the game has completed Initalization logic
			Starting,		// True while the game is in it's "OnStartGame" logic
			Playing,		// The game has started and is actively ticking
			Paused,			// True either from the editor or via gameplay programmers
			Quitting,		// True when the player has selected they want to quit the game
			ShuttingDown,	// True when the game is in it's shutdown phase
		};

		inline bool IsPlaying() const { return m_CurrentState == WorldState::Playing; }
		inline bool IsReadyForPlay() const { return m_CurrentState == WorldState::Initalized; }

		inline bool IsPaused() const { return m_CurrentState == WorldState::Paused; }

		inline WorldState GetState() const { return m_CurrentState; }

    private:

		WorldState m_CurrentState = WorldState::NONE;
		
		/** The registry and represents all active entities in this world */
		entt::registry& m_Registry;

		/** The game will allow users to specify their own update/read/write functions */
		Fling::Game* m_Game = nullptr;

		/** Flag if the world should quit or not! */
		uint8 m_ShouldQuit : 1;
    };

} // namespace Fling

#include "World.inl"