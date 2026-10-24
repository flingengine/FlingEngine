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
	 */
	class Engine : public NonCopyable
	{
	public:

		FLING_API Engine() = default;

		FLING_API ~Engine() = default;

		/**
		 * Run the engine (Startup, Tick until should stop, and shutdown)
		 * 
		 * @return uint64 0 for success, otherwise an error has occured
		 */
		template<class T_GameType>
		uint64 Run(int argc, const char* argv[]);

		/**
		 * Run the engine with developer UI (for example an editor that derives from
		 * Fling::BaseEditor) drawn every frame.
		 *
		 * @return uint64 0 for success, otherwise an error has occured
		 */
		template<class T_GameType, class T_DebugUIType>
		uint64 Run(int argc, const char* argv[]);

	private:

		/**
		 * Start any systems or subsystems that may be needed 
		*/		
		void Startup(int argc, const char* argv[]);

		/**
		* Initial tick for the engine frame
		*/		
		void Tick();

		/**
		* Shutdown all engine systems and do any necessary cleanup
		*/
		void Shutdown();

		/** Persistent world object that can be used to load levels, entities, etc */
		World* m_World = nullptr;

		/** Global registry that stores entities and components */
		entt::registry g_Registry;

		/** The implementation of the game that this engine is running. @see Fling::Game */
		Fling::Game* m_GameImpl = nullptr;

		/** Optional developer UI (for example the editor). Drawn in the ImGui subpass. */
		std::shared_ptr<Fling::IDebugUI> m_DebugUI;
	};


	template<class T_GameType, class T_DebugUIType>
	uint64 Engine::Run(int argc, const char* argv[])
	{
		static_assert(std::is_default_constructible<T_DebugUIType>::value, "T_DebugUIType requires default-constructible elements");
		static_assert(std::is_base_of<Fling::IDebugUI, T_DebugUIType>::value, "T_DebugUIType must inherit from Fling::IDebugUI");
		m_DebugUI = std::make_shared<T_DebugUIType>();

		return Run<T_GameType>(argc, argv);
	}

	template<class T_GameType>
	uint64 Engine::Run(int argc, const char* argv[])
	{
		static_assert(std::is_default_constructible<T_GameType>::value, "T_GameType requires default-constructible elements");
		static_assert(std::is_base_of<Fling::Game, T_GameType>::value, "T_GameType must inherit from Fling::Game");

		// #TODO Use a pool allocator for new
		m_GameImpl = new T_GameType();

		Startup(argc, argv);

		Tick();

		Shutdown();

		return 0;
	}
}	// namespace Fling