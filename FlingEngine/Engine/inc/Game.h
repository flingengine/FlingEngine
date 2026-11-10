#pragma once

#include "Ecs/EntityStorage.h"
#include "Platform.h"

#include <memory>

// #include "World.h"

namespace Fling
{
    class World;

    // Declared in the Physics module (PhysicsProvider.h). Engine only passes it on.
    class PhysicsProvider;

    class ModuleRegistration;

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
        virtual void Init(Fling::EntityStorage& t_Reg) = 0;

        /**
         * Add the game's component types and systems. Called from World::Init after the
         * engine modules have registered, before the type registry and the schedule are
         * built, and before the StartLevel loads. Keep it one line per feature, in one
         * register file.
         */
        virtual void Register(ModuleRegistration& t_Modules) { (void)t_Modules; }

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
        virtual void OnStartGame(Fling::EntityStorage& t_Reg) = 0;

        /**
         * Called once per frame during play, after the fixed system ticks. Optional and
         * legacy: put gameplay in systems (Register) instead.
         */
        virtual void Update(Fling::EntityStorage& t_Reg, float DeltaTime)
        {
            (void)t_Reg;
            (void)DeltaTime;
        }

        /**
         * Called by the World when play stops, before the World resets its entities.
         * After this call, the World removes all entities and loads the snapshot from
         * play start again, so you do not have to destroy the entities that play made.
         * Clean up all state that is not in the registry: member variables, cursor and
         * window changes, handles to systems outside the world.
         * The World also calls this at shutdown if the game is playing. It then does not
         * reset the entities.
         */
        virtual void OnStopGame(Fling::EntityStorage& t_Reg) = 0;

        /**
         * Called by the world when the entire application is shutting down. Good spot to do
         * cleanup of any persistent data that you may have had throughout the game lifetime.
         */
        virtual void Shutdown(Fling::EntityStorage& t_Reg) = 0;

        /**
         * Gets the owning world of this game. You can use the world to add entities to
         * 			the world. Asserts that world exists first
         */
        FORCEINLINE std::shared_ptr<World> GetWorld() const
        {
            std::shared_ptr<World> OwningWorld = m_OwningWorld.lock();
            assert(OwningWorld);
            return OwningWorld;
        }

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

        /** The world that updates this game. Set in the Engine::Startup. Weak: the world holds the game. */
        std::weak_ptr<World> m_OwningWorld;

        /** Change this value to true when the game wants to exit. The World clears it when play stops. */
        bool m_WantsToQuit = false;

        // #TODO Camera class for the game
    };
} // namespace Fling