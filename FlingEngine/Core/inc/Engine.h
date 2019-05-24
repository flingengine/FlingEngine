#pragma once

#include "Logger.h"
#include "Timing.h"
#include "Renderer.h"

namespace Fling
{
	class FLING_API Engine
	{

	public:

		Engine();

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
		/// Inital tick for the engine frame
		/// </summary>
		virtual void Tick();

		/// <summary>
		/// Shutdown all engine systems and do any necessary cleanup
		/// </summary>
		virtual void Shutdown();

	};
}	// namespace Fling