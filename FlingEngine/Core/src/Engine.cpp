#include "pch.h"
#include "Engine.h"
#include <cstdint>
#include "File.h"
#include "VulkanApp.h"
#include "Misc/CommandLine.h"
#include "Foundation.h"

namespace Fling
{
    void Engine::Startup(int argc, const char* argv[])
	{	
		Random::Init();
		Logger::Get().Init();
		
		CommandLine::Set(CommandLine::BuildFromArgs(argc, argv));
		F_LOG_TRACE("Command line args: {}\t", CommandLine::Get());

		ResourceManager::Get().Init();
		Timing::Get().Init();
        FlingConfig::Get().Init();
		Input::Init();

        F_LOG_TRACE("Fling Engine Sourcedir:  \t{}", Fling::FlingPaths::EngineSourceDir());
        F_LOG_TRACE("Fling Engine Assets dir: \t{}", Fling::FlingPaths::EngineAssetsDir());
        F_LOG_TRACE("Fling Engine Logs dir:   \t{}", Fling::FlingPaths::EngineLogDir());
        F_LOG_TRACE("Fling Engine Config dir: \t{}", Fling::FlingPaths::EngineConfigDir());

#ifdef FLING_SHIPPING
		F_LOG_TRACE("Fling Engine Config: Shipping");
#endif

        // Load command line args and any ini files
        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile(FlingPaths::EngineConfigDir() + "/EngineConf.ini");

		if (!ConfigLoaded)
		{
			F_LOG_WARN("NO EngineConf.ini has been provided! This may result in unexpected behavior from Fling!");
		}

		VulkanApp::Get().Init(
			static_cast<PipelineFlags>(PipelineFlags::DEFERRED | PipelineFlags::IMGUI),
			g_Registry,
			m_Editor
		);
		
		// Set the editor if we need to
#if WITH_EDITOR
		m_Editor->RegisterComponents(g_Registry);
#endif

		m_World = new World(g_Registry, m_GameImpl);
		m_GameImpl->m_OwningWorld = m_World;
		
#if WITH_EDITOR
		m_Editor->m_OwningWorld = m_World;
		m_Editor->m_Game = m_GameImpl;
		F_LOG_TRACE("Fling Editor: Enabled");
#else
		F_LOG_TRACE("Fling Editor: Disabled");
#endif

		Input::PreUpdate();
	}

	void Engine::Tick()
	{
        float DeltaTime = 1.0f / 60.0f;
		
		assert(m_World && m_GameImpl);		// We HAVE to have a world and a game
		
		VulkanApp& VkApp = VulkanApp::Get();
		Timing& Timing = Timing::Get();

		// Once the world is initialized it allows the users to add their own components!
		m_World->Init();

		while(!VkApp.GetCurrentWindow()->ShouldClose())
		{
            // Update timing
            Timing.Update();
            DeltaTime = Timing.GetDeltaTime();

			// Update FPS Counter
            Stats::Frames::TickStats(DeltaTime);
			
			Input::Poll();

			// World update will handle the starting, updating, and stopping of game logic
			m_World->Update(DeltaTime);

			if(m_World->ShouldQuit())
			{
				F_LOG_TRACE("World should quit! Exiting engine loop...");
				break;
			}
			
			VkApp.Update(DeltaTime, g_Registry);

			Timing.UpdateFps();
		}
	}

	void Engine::Shutdown()
	{
		// Cleanup game play stuff
		if(m_World)
		{
			m_World->Shutdown();

			if (m_GameImpl)
			{
				delete m_GameImpl;
				m_GameImpl = nullptr;
			}

			delete m_World;
			m_World = nullptr;
		}
		
		// Cleanup any resources
		Input::Shutdown();
        ResourceManager::Get().Shutdown();
		Logger::Get().Shutdown();
        FlingConfig::Get().Shutdown();
		Timing::Get().Shutdown();
		VulkanApp::Get().Shutdown(g_Registry);

		g_Registry.reset();
	}
}