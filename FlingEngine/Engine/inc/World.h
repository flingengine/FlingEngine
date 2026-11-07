#pragma once

#include "Components/AppRequests.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "FlingTypes.h"
#include "Game.h"
#include "Json.h"
#include "Level.h"
#include "Platform.h"
#include "Strings/String.h"

#include <cassert>
#include "Ecs/EntityStorage.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/TypeRegistry.h"
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

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


		explicit World(Fling::Game* t_Game);
		~World();

		/**
		 * What the modules add to this world: component types and systems. Call each
		 * module's register function with it before Init. Init then calls Game::Register,
		 * builds the TypeRegistry, and builds the schedule.
		 */
		ModuleRegistration& GetModules() { return m_Modules; }

		/** The ECS systems this world runs. Modules add them through GetModules. */
		SystemRegistry& GetSystems() { return m_Systems; }

		/** Every component type this world knows. Empty before Init. */
		const TypeRegistry& GetTypes() const { return m_Types; }

		/** How the fixed tick runs (step length, one tick per frame). Set it before Init. */
		void SetSystemRunnerConfig(const SystemRunnerConfig& t_Config) { m_RunnerConfig = t_Config; }

		/**
		 * Set the function that fills each fixed tick's input (see SystemRunner). Set it
		 * before Init. With none (for example in tests), ticks have no input.
		 */
		void SetTickInputSource(std::function<void(TickInput&)> t_Source) { m_TickInputSource = std::move(t_Source); }

		/** The schedule that Init built. Null before Init. */
		const Schedule* GetSchedule() const;

		/**
		 * The AppRequests from this frame's systems that the application handles (all but
		 * Quit, which the world handles). Clears the list.
		 */
		std::vector<AppRequest> TakeAppRequests() { return std::exchange(m_AppRequestsForApp, {}); }

		/** The schedule errors from Init. Empty if the schedule is valid. */
		const String& GetScheduleErrors() const { return m_ScheduleErrors; }

		/**
		* Initializes the world: registers components and systems, builds the system
		* schedule, calls each system's OnCreate, then loads the StartLevel that is
		* specified in the config.
		* @note		Keep explicit Init and Shutdown functions to make the startup order more readable
		*
		* @param t_StartLevel	A level to load instead of the config StartLevel (relative to the assets folder). Empty means use the config.
		* @return False if the system schedule has errors (see GetScheduleErrors). The world is then not ready for play.
		*/
		bool Init(StringView t_StartLevel = {});

		/**
		 * Run one frame: the fixed system ticks that are due, then Game::Update, then the
		 * frame systems. Systems run in the Play role during play and the Edit role otherwise.
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
		bool OutputLevelFile(StringView t_LevelToLoad);

		/**
		 * Reset the current registry and load entities/components from a JSON level file.
		 * Requires Init, which builds the type registry.
		 *
		 * @param t_LevelToLoad File path to load (relative to the assets directory)
		 * @return True on success
		 */
		bool LoadLevelFile(StringView t_LevelToLoad);

		/** The entities, components, and singletons of this world. */
		FORCEINLINE EntityStorage& GetStorage() { return m_Storage; }

		FORCEINLINE const EntityStorage& GetStorage() const { return m_Storage; }

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

        /**
         * Write all saved components on living entities to a level document. Gives each
         * entity an EntityGuid first if it has none, so references between entities save.
         */
        bool SaveToJson(Json& t_OutDoc, StringView t_Title);

        /** A new random EntityGuid value that no entity in this world uses. */
        uint64 MakeEntityGuid() const;

        /** Remove all entities, then make the entities in a level document */
        bool LoadFromJson(const Json& t_Doc, StringView t_SourceName);

        WorldState m_CurrentState = WorldState::NONE;

        PlayRequest m_PendingRequest = PlayRequest::None;

        /** The entities as they were when play started. Empty when there is no play session. */
        Json m_PlaySnapshot;

        /** True if the play snapshot holds data to restore */
        bool m_HasPlaySnapshot = false;

        /** @see SetGameQuitStopsPlay */
        bool m_GameQuitStopsPlay = false;

        /** All entities in this world. Declared before the systems and runner, so it is destroyed after them. */
		EntityStorage m_Storage;

		/** The systems this world runs. */
		SystemRegistry m_Systems;

		/** @see GetModules. Declared after m_Systems, which it refers to. */
		ModuleRegistration m_Modules{ m_Systems };

		/** @see GetTypes. Built by Init. */
		TypeRegistry m_Types;

		/** True after Init built m_Types. A level cannot load or save before that. */
		bool m_TypesBuilt = false;

		/** @see SetSystemRunnerConfig */
		SystemRunnerConfig m_RunnerConfig;

		/** @see SetTickInputSource */
		std::function<void(TickInput&)> m_TickInputSource;

		/** Runs the schedule. Made by Init when the schedule has no errors. */
		std::unique_ptr<SystemRunner> m_Runner;

		/** @see GetScheduleErrors */
		String m_ScheduleErrors;

		/** @see TakeAppRequests */
		std::vector<AppRequest> m_AppRequestsForApp;

		/** Take the AppRequests queue: handle Quit, and keep the rest for the application. */
		void ProcessAppRequests();

		/** The game will allow users to specify their own update/read/write functions */
		Fling::Game* m_Game = nullptr;

		/** Flag if the world should quit or not! */
		uint8 m_ShouldQuit : 1;
	};

} // namespace Fling
