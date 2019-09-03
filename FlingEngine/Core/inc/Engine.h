#pragma once

#include "Platform.h"
#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "ComponentManager.h"
#include "FlingConfig.h"
#include "NonCopyable.hpp"
#include "World.h"
#include <nlohmann/json.hpp>

namespace Fling
{
	class FLING_API Engine : public NonCopyable
	{
	public:

		Engine() = default;

        Engine(int argc, char* argv[]);

		~Engine() = default;

		/// <summary>
		/// Run the engine
		/// </summary>
		/// <returns>Error code or 0 for success</returns>
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
		/// Shutdown all engine systems and do any necessary cleanup
		/// </summary>
		void Shutdown();

        int m_CmdLineArgCount = 0;
        char** m_CmdLineArgs = nullptr;
	};
}	// namespace Fling