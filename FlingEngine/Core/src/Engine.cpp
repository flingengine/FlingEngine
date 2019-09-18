#include "pch.h"
#include "Engine.h"
#include <cstdint>
#include "File.h"

namespace Fling
{
    Engine::Engine(int argc, char* argv[])
        : Engine()
    {
        m_CmdLineArgCount = argc;
        m_CmdLineArgs = argv;
    }

	UINT64 Engine::Run()
	{
		Startup();

		Tick();

		Shutdown();

		return 0;
	}

	void Engine::Startup()
	{
		Random::Init();
		Logger::Get().Init();
        ResourceManager::Get().Init();
		Timing::Get().Init();
        FlingConfig::Get().Init();
		Input::Init();

        F_LOG_TRACE("Fling Engine Sourcedir:  \t{}", Fling::FlingPaths::EngineSourceDir());
        F_LOG_TRACE("Fling Engine Assets dir: \t{}", Fling::FlingPaths::EngineAssetsDir());
        F_LOG_TRACE("Fling Engine Logs dir:   \t{}", Fling::FlingPaths::EngineLogDir());
        F_LOG_TRACE("Fling Engine Config dir: \t{}", Fling::FlingPaths::EngineConfigDir());

	#ifdef FLING_SHIPPING
		F_LOG_TRACE("Fling Engine: Shipping");
	#endif

        // Load command line args and any ini files
        //#TODO Handle command line args
        UINT32 ArgsLoaded = FlingConfig::Get().LoadCommandLineOpts(m_CmdLineArgCount, m_CmdLineArgs);
        (void)(ArgsLoaded);

        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile(FlingPaths::EngineConfigDir() + "/EngineConf.ini");

        Renderer::Get().CreateGameWindow(
            ConfigLoaded ? FlingConfig::GetInt("Engine", "WindowWidth") : FLING_DEFAULT_WINDOW_WIDTH,
            ConfigLoaded ? FlingConfig::GetInt("Engine", "WindowHeight") : FLING_DEFAULT_WINDOW_WIDTH
        );

		Renderer::Get().Init();

		ComponentManager::Get().Init();

		m_World = new World();
	}

	void Engine::Tick()
	{
        // Calculate a fall back delta time in case the engine ever gets out of sync
        const static float FallbackDeltaTime = 1.0f / 60.0f;
        const static float MaxDeltaTime = 1.0f;

        float DeltaTime = FallbackDeltaTime;
		
		assert(m_World);		// We HAVE to have a world
		
		Renderer& Renderer = Renderer::Get();
		Timing& Timing = Timing::Get();

		while(!Renderer.GetCurrentWindow()->ShouldClose())
		{
			Renderer.Tick();

			m_World->Update(DeltaTime);
			if(m_World->ShouldQuit())
			{
				F_LOG_TRACE("World should quit! Exiting engine loop...");
				break;
			}

			Renderer.DrawFrame();

            // Update timing
			Timing.Update();
            DeltaTime = Timing.GetDeltaTime();
            
            // If delta time is greater than 1 second, simulate it as 1/60 FPS 
            // because we can assume that it is like that because of debugging
            if (DeltaTime >= MaxDeltaTime)
            {
				DeltaTime = FallbackDeltaTime;
            }
		}

		// Any waiting that we may need to do before the shutdown function should go here
		Renderer.PrepShutdown();
	}

	void Engine::Shutdown()
	{
		ComponentManager::Get().Shutdown();
		
		if(m_World)
		{
			m_World->Shutdown();
			delete m_World;
			m_World = nullptr;
		}
		
		// Cleanup any resources
		Input::Shutdown();
        ResourceManager::Get().Shutdown();
		Logger::Get().Shutdown();
        FlingConfig::Get().Shutdown();
		Timing::Get().Shutdown();
		Renderer::Get().Shutdown();
	}
}