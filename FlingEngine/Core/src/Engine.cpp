#include "pch.h"
#include "Engine.h"

namespace Fling
{
	Engine::Engine()
	{
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

        ResourceManager::instance().Init();
		Logger::instance().Init();
		Timing::instance().Init();
        Renderer::instance().CreateGameWindow(FLING_DEFAULT_WINDOW_WIDTH, FLING_DEFAULT_WINDOW_HEIGHT);
		Renderer::instance().Init();
	}

	void Engine::Tick()
	{
        const static float FallbackDeltaTime = 1.0f / 60.0f;
        const static float MaxDeltaTime = 1.0f;

        float deltaTime = FallbackDeltaTime;
        float totalTime = 0.0f;

		while( !glfwWindowShouldClose( Renderer::instance().Window() ) )
		{
            // Update events
			glfwPollEvents();

            // #TODO Update any game play systems here

            // Render
            Renderer::instance().DrawFrame();
            
            // Update timing
            Timing::instance().Update();
            deltaTime = Timing::instance().GetDeltaTime();
			totalTime = Timing::instance().GetTimef();

            // #TODO If delta time is greater than 1 second, simulate it as 1/60 FPS 
            // because we can assume that it is like that because of debugging
            if (deltaTime >= MaxDeltaTime)
            {
                deltaTime = FallbackDeltaTime;
            }
		}

        // Pre-shutdown options here
        Renderer::instance().PrepShutdown();
	}

	void Engine::Shutdown()
	{
		// Cleanup any resources
        ResourceManager::instance().Shutdown();
		Logger::instance().Shutdown();
		Timing::instance().Shutdown();
		Renderer::instance().Shutdown();
	}
}