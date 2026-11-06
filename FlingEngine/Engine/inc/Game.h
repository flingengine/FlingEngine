#pragma once

#include "Platform.h"

#include <entt/entity/registry.hpp>
#include <memory>
#include <string>
//#include "World.h" 

namespace Fling
{
	class World;

	// Declared in the Physics module (PhysicsProvider.h). Engine only passes it on.
	class PhysicsProvider;

	class SystemRegistry;

	/**
	 * The game class is mean to be overridden on a per-game instance.
	 *          It provides an interface for users to add their own System calls
	 *          in the update, read, write, etc
	 * @see World
	 */
	class Game
	{
		// Friend class to the engine so that we can ensure the proper world is setup
		friend class Engine;

        // The world runs the play session, and clears m_WantsToQuit when play stops
        friend class World;

    public:
		Game(const Game&) = delete;
		Game& operator=(const Game&) = delete;

		/**
		* Initialization of a Game called by the world. This is a good place to bind
		* any game input callbacks, or to Load any Lua scripts that need a "Start" method
		*/
		virtual void Init(entt::registry& t_Reg) = 0;

		/**
		 * Register game-specific serializable components. Called from World::Init
		 * before Seal() and before the StartLevel is loaded.
		 */
		virtual void RegisterComponents() {}

		/**
		 * Add the game's ECS systems, phases, and orders. Called from World::Init after
		 * RegisterComponents, before the schedule is built. A game with many systems
		 * defines this in one register file that includes each system header.
		 */
		virtual void RegisterSystems(SystemRegistry& t_Systems) { (void)t_Systems; }

        /**
         * Return the physics provider that this game uses, for example a provider
         * for a physics library that the game brings. Return null (the default) to
         * use the provider of this build (FLING_PHYSICS_PROVIDER). Called one time,
         * before Init.
         */
        virtual std::shared_ptr<PhysicsProvider> CreatePhysicsProvider() { return nullptr; }

        /**
         * Called by the World when play starts. The World keeps a snapshot of all
         * entities just before this call. Make the entities and state for play here.
         * In the editor, play can start and stop many times in one run, so reset all
         * per-play state here or in OnStopGame.
         */
        virtual void OnStartGame(entt::registry& t_Reg) = 0;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		virtual void Update(entt::registry& t_Reg, float DeltaTime) = 0;

        /**
         * Called by the World when play stops, before the World resets its entities.
         * After this call, the World removes all entities and loads the snapshot from
         * play start again, so you do not have to destroy the entities that play made.
         * Clean up all state that is not in the registry: member variables, cursor and
         * window changes, handles to systems outside the world.
         * The World also calls this at shutdown if the game is playing. It then does not
         * reset the entities.
         */
        virtual void OnStopGame(entt::registry& t_Reg) = 0;

		/** 
		* Called by the world when the entire application is shutting down. Good spot to do 
		* cleanup of any persistent data that you may have had throughout the game lifetime. 
		*/
		virtual void Shutdown(entt::registry& t_Reg) = 0;

		/**
		 * Gets the owning world of this game. You can use the world to add entities to
		 * 			the world. Asserts that world exists first
		 */
		FORCEINLINE World* GetWorld() const { assert(m_OwningWorld); return m_OwningWorld; }

        /**
         * If true then this game wants to exit. In editor play this stops play instead
         * of the application (see World::SetGameQuitStopsPlay).
         */
        FORCEINLINE bool WantsToQuit() const { return m_WantsToQuit; } 

	protected:

		// You really should not be implementing the game's ctor because it 
		// is not explicitly managed by the Engine's lifecycle
		Game() = default;
		virtual ~Game() = default;
		
		/** The world that updates this game. Set in the Engine::Startup */
		World* m_OwningWorld = nullptr;

        /** Change this value to true when the game wants to exit. The World clears it when play stops. */
        bool m_WantsToQuit = false;

		// #TODO Camera class for the game
	};
}   // namespace Fling