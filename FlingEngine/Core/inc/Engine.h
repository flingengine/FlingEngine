#pragma once

#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "FlingConfig.h"

namespace Fling
{
	class Engine
	{

	public:

		Engine();

        Engine(int argc, char* argv[]);

		~Engine();

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

	};
}	// namespace Fling