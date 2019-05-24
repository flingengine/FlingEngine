#pragma once

#include "Logger.h"
#include "Timing.h"
#include "Renderer.h"

namespace Fling
{
	class FLING_API Engine
	{

	public:


		Engine( const int t_width = 800, const int t_height = 600 );

		~Engine();

		/// <summary>
		/// Run the engine
		/// </summary>
		/// <returns>Error code or 0 for success</returns>
		UINT64 Run();

	private:

		virtual void Startup();

		virtual void Tick();

		virtual void Shutdown();

	};
}	// namespace Fling