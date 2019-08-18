#include "pch.h"
#include "Engine.h"

namespace Fling
{
	Engine::Engine()
	{
	}

    Engine::Engine(int argc, char* argv[])
        : Engine()
    {
        m_CmdLineArgCount = argc;
        m_CmdLineArgs = argv;
    }

	Engine::~Engine()
	{
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

        FlingConfig::Get().Init();

        F_LOG_TRACE("Fling Engine Sourcedir:  \t{}", Fling::FlingPaths::EngineSourceDir());
        F_LOG_TRACE("Fling Engine Assets dir: \t{}", Fling::FlingPaths::EngineAssetsDir());
        F_LOG_TRACE("Fling Engine Logs dir:   \t{}", Fling::FlingPaths::EngineLogDir());
        F_LOG_TRACE("Fling Engine Config dir: \t{}", Fling::FlingPaths::EngineConfigDir());

        // Load command line args and any ini files
        //#TODO Handle command line args
        UINT32 ArgsLoaded = FlingConfig::Get().LoadCommandLineOpts(m_CmdLineArgCount, m_CmdLineArgs);
        (void)(ArgsLoaded);

        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile(FlingPaths::EngineConfigDir() + "/EngineConf.ini");

		Timing::Get().Init();

        Renderer::Get().CreateGameWindow(
            ConfigLoaded ? FlingConfig::Get().GetInt("Engine", "WindowWidth") : FLING_DEFAULT_WINDOW_WIDTH,
            ConfigLoaded ? FlingConfig::Get().GetInt("Engine", "WindowHeight") : FLING_DEFAULT_WINDOW_WIDTH
        );

		Renderer::Get().Init();
	}

	void Engine::Tick()
	{
        // Calculate a fallback delta time in case the engine ever gets out of sync
        const static float FallbackDeltaTime = 1.0f / 60.0f;
        const static float MaxDeltaTime = 1.0f;

        float deltaTime = FallbackDeltaTime;

		while( !glfwWindowShouldClose( Renderer::Get().Window() ) )
		{
            // Update events
			glfwPollEvents();

            // #TODO Provide a game play layer that we can use to put any application
            // specific update systems in (i.e. an actual scene graph model)

            // Renderer
            Renderer::Get().DrawFrame();
            
            // Update timing
            Timing::Get().Update();
            deltaTime = Timing::Get().GetDeltaTime();
            
            // If delta time is greater than 1 second, simulate it as 1/60 FPS 
            // because we can assume that it is like that because of debugging
            if (deltaTime >= MaxDeltaTime)
            {
                deltaTime = FallbackDeltaTime;
            }
		}

        // Pre-shutdown options here
        Renderer::Get().PrepShutdown();
	}

	void Engine::Shutdown()
	{
		// Cleanup any resources
        ResourceManager::Get().Shutdown();
		Logger::Get().Shutdown();
        FlingConfig::Get().Shutdown();
		Timing::Get().Shutdown();
		Renderer::Get().Shutdown();
	}
}