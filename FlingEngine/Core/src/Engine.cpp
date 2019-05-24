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
		Utils::Random::Init();

		Logger::instance().Init();
		Timing::instance().Init();
		Renderer::instance().Init();
		Renderer::instance().CreateGameWindow( FLING_DEFAULT_WINDOW_WIDTH, FLING_DEFAULT_WINDOW_HEIGHT );
	}

	void Engine::Tick()
	{
		while( !glfwWindowShouldClose( Renderer::instance().Window() ) )
		{
			glfwPollEvents();
			Timing::instance().Update();

			float deltaTime = Timing::instance().GetDeltaTime();
			float totalTime = Timing::instance().GetTimef();
		}
	}

	void Engine::Shutdown()
	{
		// Cleanup any resources
		Logger::instance().Shutdown();
		Timing::instance().Shutdown();
		Renderer::instance().Shutdown();
	}
}