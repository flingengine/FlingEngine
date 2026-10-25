#pragma once

#include "Platform.h"
#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "FlingConfig.h"
#include "NonCopyable.hpp"
#include "World.h"
#include <entt/entity/registry.hpp>
#include <csignal>

#include "MovingAverage.hpp"
#include "Stats.h"
#include "Game.h"
#include "IDebugUI.h"
#include "ExitCode.h"
#include "LaunchOptions.h"
#include <chrono>
#include <memory>
#include <type_traits>

/**
 * Triggers a breakpoint at the call site, halting execution if a debugger is attached.
 * Works across compilers/platforms so callers don't need their own #ifdef ladder.
 */
#if defined( _MSC_VER )
	#define F_DEBUG_BREAK() __debugbreak()
#elif defined( __clang__ )
	#define F_DEBUG_BREAK() __builtin_debugtrap()
#elif defined( __MINGW32__ )
	// MinGW-w64 targets Windows, which has no SIGTRAP to raise()
	#define F_DEBUG_BREAK() asm("int $3")
#elif defined( __GNUC__ )
	#define F_DEBUG_BREAK() raise( SIGTRAP )
#else
	#define F_DEBUG_BREAK()
#endif

namespace Fling
{
	/**
	 * Core engine class of Fling. This is where the core update loop lives
	 * along with all startup/shutdown ordering.
	 *
	 * The loop does not need a window: with --null-render the engine makes no
	 * window and no GPU device, and ticks with a fixed delta. See LaunchOptions
	 * for the launch flags.
	 */
	class Engine : public NonCopyable
	{
	public:

		FLING_API Engine() = default;

		FLING_API ~Engine() = default;

		/**
		 * Run the engine (Startup, Tick until should stop, and shutdown)
		 *
		 * @return The process exit code, one of the Fling::ExitCode values
		 */
		template<class T_GameType>
		int Run(int argc, const char* argv[]);

		/**
		 * Run the engine with developer UI (for example an editor that derives from
		 * Fling::BaseEditor) drawn every frame. Under --null-render there is no
		 * developer UI.
		 *
		 * @return The process exit code, one of the Fling::ExitCode values
		 */
		template<class T_GameType, class T_DebugUIType>
		int Run(int argc, const char* argv[]);

	private:

		/**
		 * Start any systems or subsystems that may be needed
		 *
		 * @return ExitCode::BadArguments if a launch flag is not valid, otherwise ExitCode::Success
		*/
		ExitCode Startup(int argc, const char* argv[]);

		/**
		* Run the main loop until the window closes, the world quits, or --frames
		* ticks have run
		*/
		void Tick();

		/** True while the main loop must run one more tick. */
		bool ShouldTick() const;

		/**
		* Shutdown all engine systems and do any necessary cleanup
		*
		* @param t_Code The exit code of this run, written in the exit summary
		* @return t_Code
		*/
		ExitCode Shutdown(ExitCode t_Code);

		/** Write a one-line summary of this run to stdout, in the --log-format. */
		void PrintExitSummary(ExitCode t_Code) const;

		/** Persistent world object that can be used to load levels, entities, etc */
		World* m_World = nullptr;

		/** Global registry that stores entities and components */
		entt::registry g_Registry;

		/** The implementation of the game that this engine is running. @see Fling::Game */
		Fling::Game* m_GameImpl = nullptr;

		/** Optional developer UI (for example the editor). Drawn in the ImGui subpass. */
		std::shared_ptr<Fling::IDebugUI> m_DebugUI;

		/** Launch flags read from the command line in Startup */
		LaunchOptions m_Options;

		/** The seed of the random number generator. Pass it to --seed to repeat this run. */
		uint32 m_Seed = 0;

		/** Number of ticks that the main loop has run */
		uint64 m_TicksRun = 0;

		/** True after Startup started the engine systems, which Shutdown must then stop */
		bool m_SystemsStarted = false;

		/** Real time when Startup began, for the exit summary */
		std::chrono::steady_clock::time_point m_StartTime;
	};


	template<class T_GameType, class T_DebugUIType>
	int Engine::Run(int argc, const char* argv[])
	{
		static_assert(std::is_default_constructible<T_DebugUIType>::value, "T_DebugUIType requires default-constructible elements");
		static_assert(std::is_base_of<Fling::IDebugUI, T_DebugUIType>::value, "T_DebugUIType must inherit from Fling::IDebugUI");
		m_DebugUI = std::make_shared<T_DebugUIType>();

		return Run<T_GameType>(argc, argv);
	}

	template<class T_GameType>
	int Engine::Run(int argc, const char* argv[])
	{
		static_assert(std::is_default_constructible<T_GameType>::value, "T_GameType requires default-constructible elements");
		static_assert(std::is_base_of<Fling::Game, T_GameType>::value, "T_GameType must inherit from Fling::Game");

		// #TODO Use a pool allocator for new
		m_GameImpl = new T_GameType();

		const ExitCode StartupCode = Startup(argc, argv);
		if (StartupCode == ExitCode::Success)
		{
			Tick();
		}

		return ToInt(Shutdown(StartupCode));
	}
}	// namespace Fling
