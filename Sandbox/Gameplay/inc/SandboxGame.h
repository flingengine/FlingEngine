#pragma once

#include "Game.h"

namespace Sandbox
{
	class Game : public Fling::Game
	{
		void Init() override;

		/* Called when the engine is shutting down */
		void Shutdown() override;

		/**
		* Update is called every frame. Call any system updates for your gameplay systems inside of here
		*/
		void Update(float DeltaTime) override;

		/**
		 * Read is called when a level file is read from disk. Here is where you can
		 * read in a file stream and based the data inside it, initalize your gameplay
		 * systems
		 */
		bool Read(/* TODO: Cereal stream */) override;

		/**
		 * TODO
		 */
		bool Write(/* TODO: Cereal stream  */) override;
	};
}	// namespace Sandbox
