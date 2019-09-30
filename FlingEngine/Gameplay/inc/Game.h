#pragma once

#include "NonCopyable.hpp"
#include <string>
#include <entt/entity/registry.hpp>
//#include "World.h" 

namespace Fling
{
	/**
	 * @brief   The game class is mean to be overridden on a per-game instance.
	 *          It provides an interface for users to add their own System calls
	 *          in the update, read, write, etc
	 * @see World
	 */
	class Game : public NonCopyable
	{
		// Friend class to the engine so that we can ensure the proper world is setup
		friend class Engine;

	public:
		
		/**
		 * Called before the first Update tick. 
		 */
		virtual void Init(entt::registry& t_Reg) = 0;

		/* Called when the engine is shutting down */
		virtual void Shutdown(entt::registry& t_Reg) = 0;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		virtual void Update(entt::registry& t_Reg, float DeltaTime) = 0;

		/**
		 * @brief 	Gets the owning world of this game. You can use the world to add entities to
		 * 			the world. Asserts that world exists first
		 * 
		 * @return FORCEINLINE* GetWorld 
		 */
		FORCEINLINE class World* GetWorld() const { assert(m_OwningWorld); return m_OwningWorld; }

		/**
		 * @brief If true then this game wants to texit the application entirely.
		 * 
		 * @return FORCEINLINE WantsToQuit
		 */
		FORCEINLINE bool WantsToQuit() const { return m_WantsToQuit; } 

	protected:

		// You really should not be implementing the game's ctor
		Game() = default;
		virtual ~Game() = default;
		
		/** The world that updates this game. Set in the Engine::Startup */
		class World* m_OwningWorld = nullptr;

		/** Change this value to true when the game is ready to exit the application entirely */
		bool m_WantsToQuit = false;
	};
}   // namespace Fling