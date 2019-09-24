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

		/**
		 * Read is called when a level file is read from disk. Here is where you can
		 * read in a file stream and based the data inside it, initalize your gameplay
		 * systems
		 */
		bool Read(entt::registry& t_Reg/* TODO: Cereal stream */) override;

		/**
		 * TODO
		 */
		bool Write(entt::registry& t_Reg/* TODO: Cereal stream  */) override;
	};
}	// namespace Sandbox
