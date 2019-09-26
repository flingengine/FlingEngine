#pragma once

#include "Game.h"

namespace Sandbox
{
	class Game : public Fling::Game
	{
		void Init(entt::registry& t_Reg) override;

		/* Called when the engine is shutting down */
		void Shutdown(entt::registry& t_Reg) override;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		void Update(entt::registry& t_Reg, float DeltaTime) override;
	};
}	// namespace Sandbox
