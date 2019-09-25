#pragma once

#include "Platform.h"
#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "FlingConfig.h"
#include "NonCopyable.hpp"
#include "World.h"
#include <nlohmann/json.hpp>

namespace Fling
{
	/**
	 * @brief Core engine class of Fling. This is where the core update loop lives 
	 * along with all startup/shutdown ordering. 
	 */
	class FLING_API Engine : public NonCopyable
	{
	public:

		Engine() = default;

        Engine(int argc, char* argv[]);

		~Engine() = default;

		/**
		 * @brief Run the engine (Startup, Tick until should stop, and shutodwn)
		 * 
		 * @return UINT64 0 for success, otherwise an error has occured
		 */
		UINT64 Run();

	private:

		/// <summary>
		/// Start any systems or subsystems that may be needed
		/// </summary>
		void Startup();

		/// <summary>
		/// Initial tick for the engine frame
		/// </summary>
		void Tick();

		/// <summary>
		/// Updates fps count
		/// </summary>
		void UpdateFps(const float& totalTime);
		
		/// <summary>
		/// Shutdown all engine systems and do any necessary cleanup
		/// </summary>
		void Shutdown();

        int m_CmdLineArgCount = 0;
        char** m_CmdLineArgs = nullptr;

		/** Persistant world object that can be used to load levels, entities, etc */
		World* m_World = nullptr;

		/** updates the timer for this frame */
		float m_fpsTimeElapsed = 0.0f;
		int m_fpsFrameCount = 0; 
	};
}	// namespace Fling