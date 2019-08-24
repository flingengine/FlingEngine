#pragma once

#include "Platform.h"
#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "FlingConfig.h"
#include "NonCopyable.hpp"
#include "Game.h"

#include <memory>

namespace Fling
{
	class FLING_API Engine : public NonCopyable
	{

	public:

		Engine();

        Engine(int argc, char* argv[]);

		~Engine();

		/***
		* @brief	Set the game type to that which you want! 
		*/
		template<class GameType>
		void SetGame()
		{
			m_GameInstance = std::make_unique<GameType>();
		}

		/// <summary>
		/// Run the engine
		/// </summary>
		/// <returns>Error code or 0 for success</returns>
		UINT64 Run();

	private:

		/// <summary>
		/// Start any systems or subsystems that may be needed
		/// </summary>
		virtual void Startup();

		/// <summary>
		/// Initial tick for the engine frame
		/// </summary>
		virtual void Tick();

		/// <summary>
		/// Shutdown all engine systems and do any necessary cleanup
		/// </summary>
		virtual void Shutdown();

        int m_CmdLineArgCount = 0;
        char** m_CmdLineArgs = nullptr;

		/** The game instance */
		std::unique_ptr<Game> m_GameInstance;

	};
}	// namespace Fling