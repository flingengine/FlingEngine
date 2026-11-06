#pragma once

#include "FlingTypes.h"
#include "Game.h"
#include "Json.h"
#include "Level.h"
#include "Platform.h"

#include <cassert>
#include <entt/entity/registry.hpp>
#include <string>

namespace Fling
{
	/**
	* The world holds all active levels in the game. There will only ever be exactly one World
	* instance at any given time.
	* @see Level
	*/
	class World
	{
	public:
		World(const World&) = delete;
		World& operator=(const World&) = delete;


		explicit World(entt::registry& t_Reg, Fling::Game* t_Game);

		/**
		* Initializes the world. Loads the StartLevel that is specified in the config.
		* @note		Keep explicit Init and Shutdown functions to make the startup order more readable
		*
		* @param t_StartLevel	A level to load instead of the config StartLevel (relative to the assets folder). Empty means use the config.
		*/
		void Init(const std::string& t_StartLevel = {});

		/**
		 * Tick all active levels in the world and upates any Lua scripts that have Update functions
		 *
		 * @param t_DeltaTime   Time between previous frame and the current one.
		 */
		void Update(float t_DeltaTime);

		/**
		 * Called just before destruction.
		 */
		void Shutdown();

        /**
         * Ask the world to start play. The request runs at the start of the next Update,
         * not during the call. Before OnStartGame, the world keeps a snapshot of all
         * entities, so that RequestGameStop can put the world back.
         * Has no effect if the world is not ready for play.
         */
        void RequestGameStart();

        /**
         * Ask the world to stop play. The request runs at the start of the next Update,
         * not during the call. The world calls OnStopGame, then removes all entities and
         * loads the snapshot from RequestGameStart again.
         * Has no effect if the world is not playing.
         */
        void RequestGameStop();

        /**
         * Set what happens when the game asks to quit (Game::WantsToQuit) during play.
         * When true, play stops and the application keeps running (editor play).
         * When false (the default), the application quits.
         */
        void SetGameQuitStopsPlay(bool t_StopsPlay) { m_GameQuitStopsPlay = t_StopsPlay; }

        /**
		 * Check if the world wants to exit the program.
		 * @see Engine::Tick
		 *
		 * @return True if the world has signaled for exit
		 */
        FORCEINLINE bool ShouldQuit() const
        {
            assert(m_Game);
            return m_ShouldQuit || (m_Game->WantsToQuit() && !m_GameQuitStopsPlay);
        }

        /**
		 * Serialize all registered components on living entities to a JSON level file.
		 *
		 * @param t_LevelToLoad File path to write (relative to the assets directory)
		 * @return True on success
		 */
		bool OutputLevelFile(const std::string& t_LevelToLoad);

		/**
		 * Reset the current registry and load entities/components from a JSON level file.
		 * Requires ComponentTypeRegistry to be sealed.
		 *
		 * @param t_LevelToLoad File path to load (relative to the assets directory)
		 * @return True on success
		 */
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

        /** True if a start or stop request waits for the next Update */
        inline bool HasPendingPlayRequest() const { return m_PendingRequest != PlayRequest::None; }

        inline bool IsPaused() const { return m_CurrentState == WorldState::Paused; }

		inline WorldState GetState() const { return m_CurrentState; }

	private:
        enum class PlayRequest : uint8
        {
            None,
            Start,
            Stop,
        };

        /** Run the pending start or stop request, if there is one */
        void ProcessPlayRequest();

        /** Keep a snapshot of the entities, then call Game::OnStartGame */
        void StartGame();

        /**
         * Call Game::OnStopGame.
         * @param t_RestoreSnapshot		If true, replace all entities with the snapshot from StartGame
         */
        void StopGame(bool t_RestoreSnapshot);

        /** Write all registered components on living entities to a level document */
        bool SaveToJson(Json& t_OutDoc, const std::string& t_Title) const;

        /** Remove all entities, then make the entities in a level document */
        bool LoadFromJson(const Json& t_Doc, const std::string& t_SourceName);

        WorldState m_CurrentState = WorldState::NONE;

        PlayRequest m_PendingRequest = PlayRequest::None;

        /** The entities as they were when play started. Empty when there is no play session. */
        Json m_PlaySnapshot;

        /** True if the play snapshot holds data to restore */
        bool m_HasPlaySnapshot = false;

        /** @see SetGameQuitStopsPlay */
        bool m_GameQuitStopsPlay = false;

        /** The registry and represents all active entities in this world */
		entt::registry& m_Registry;

		/** The game will allow users to specify their own update/read/write functions */
		Fling::Game* m_Game = nullptr;

		/** Flag if the world should quit or not! */
		uint8 m_ShouldQuit : 1;
	};

} // namespace Fling
