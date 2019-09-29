#pragma once

#include "Game.h"

namespace Sandbox
{
	/**
	 * @brief Custom game class that will have control of it's gameplay systems.
	 */
	class Game : public Fling::Game
	{
		/**
		 * @brief 	Called before the first gameplay loop tick. 
		 * 			Do any initalization for custom gameplay systems here. 
		 */
		void Init(entt::registry& t_Reg) override;

		/* Called when the engine is shutting down */
		void Shutdown(entt::registry& t_Reg) override;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		void Update(entt::registry& t_Reg, float DeltaTime) override;
	};
}	// namespace Sandbox