#pragma once

#include "Platform.h"
#include "Logger.h"
#include "Timing.h"
#include "ResourceManager.h"
#include "FlingConfig.h"
#include "NonCopyable.hpp"
#include "World.h"
#include <nlohmann/json.hpp>
#include <entt/entity/registry.hpp>

#include "MovingAverage.hpp"
#include "Stats.h"
#include "Game.h"

#if WITH_EDITOR

#include "BaseEditor.h"

#endif	// WITH_EDITOR

namespace Fling
{
	class VulkanApp;

	/**
	 * @brief Core engine class of Fling. This is where the core update loop lives 
	 * along with all startup/shutdown ordering. 
	 */
	class Engine : public NonCopyable
	{
	public:

		FLING_API Engine() = default;

		FLING_API ~Engine() = default;

		/**
		 * @brief Run the engine (Startup, Tick until should stop, and shutdown)
		 * 
		 * @return UINT64 0 for success, otherwise an error has occured
		 */
#if WITH_EDITOR
		template<class T_GameType, class T_EditorType = Fling::BaseEditor>
		FLING_API UINT64 Run();
#else
		template<class T_GameType>
		FLING_API UINT64 Run();
#endif

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

		/** Persistent world object that can be used to load levels, entities, etc */
		World* m_World = nullptr;

		/** Global registry that stores entities and components */
		entt::registry g_Registry;

		/** The implementation of the game that this engine is running. @see Fling::Game */
		Fling::Game* m_GameImpl = nullptr;

#if WITH_EDITOR

		/** Overrideable editor class for Drawing with ImGUI. Drawn in Renderer::DrawFrame */
		std::shared_ptr<Fling::BaseEditor> m_Editor;

#endif
	};


#if WITH_EDITOR
	template<class T_GameType, class T_EditorType>
	FLING_API UINT64 Engine::Run()
#else
	template<class T_GameType>
	FLING_API UINT64 Engine::Run()
#endif

	{
		static_assert(std::is_default_constructible<T_GameType>::value, "T_GameType requires default-constructible elements");
		static_assert(std::is_base_of<Fling::Game, T_GameType>::value, "T_GameType must inherit from Fling::Game");

		// #TODO Use a pool allocator for new
		m_GameImpl = new T_GameType();
		
#if WITH_EDITOR
		static_assert(std::is_default_constructible<T_EditorType>::value, "T_EditorType requires default-constructible elements");
		static_assert(std::is_base_of<Fling::BaseEditor, T_EditorType>::value, "T_EditorType must inherit from Fling::BaseEditor");
		m_Editor = std::make_shared<T_EditorType>();
#endif
		Startup();

		Tick();

		Shutdown();

		return 0;
	}
}	// namespace Fling