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
		friend class Engine;

	public:
		
		/**
		 * TODO
		 */
		virtual void Init(entt::registry& t_Reg) = 0;

		/* Called when the engine is shutting down */
		virtual void Shutdown(entt::registry& t_Reg) = 0;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		virtual void Update(entt::registry& t_Reg, float DeltaTime) = 0;

		FORCEINLINE class World* GetWorld() const { return m_OwningWorld; }

	protected:

		// You really should not be implementing the game's ctor
		Game() = default;
		virtual ~Game() = default;
		
		class World* m_OwningWorld = nullptr;
	};
}   // namespace Fling