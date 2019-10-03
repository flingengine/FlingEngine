#include "pch.h"
#include "Engine.h"
#include <cstdint>
#include "File.h"

namespace Fling
{
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
        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile(FlingPaths::EngineConfigDir() + "/EngineConf.ini");

        Renderer::Get().CreateGameWindow(
            ConfigLoaded ? FlingConfig::GetInt("Engine", "WindowWidth") : FLING_DEFAULT_WINDOW_WIDTH,
            ConfigLoaded ? FlingConfig::GetInt("Engine", "WindowHeight") : FLING_DEFAULT_WINDOW_WIDTH
        );

		Renderer::Get().Init();

		m_World = new World(g_Registry, m_GameImpl);
		m_GameImpl->m_OwningWorld = m_World;
	}

	void Engine::Tick()
	{
        // Calculate a fall back delta time in case the engine ever gets out of sync
        const static float FallbackDeltaTime = 1.0f / 60.0f;
        const static float MaxDeltaTime = 1.0f;

        float DeltaTime = FallbackDeltaTime;
		
		assert(m_World && m_GameImpl);		// We HAVE to have a world
		
		Renderer& Renderer = Renderer::Get();
		Timing& Timing = Timing::Get();

		m_World->Init();

		while(!Renderer.GetCurrentWindow()->ShouldClose())
		{
			
			Renderer.Tick();

			m_World->Update(DeltaTime);
		
			if(m_World->ShouldQuit())
			{
				F_LOG_TRACE("World should quit! Exiting engine loop...");
				break;
			}

			//// Update imGui
			ImGuiIO& io = ImGui::GetIO();

			io.DisplaySize = ImVec2(
				static_cast<float>(Renderer.GetCurrentWindow()->GetWidth()), 
				static_cast<float>(Renderer.GetCurrentWindow()->GetHeight()));

			io.DeltaTime = Timing.GetTimeSinceStart();

			io.MousePos = ImVec2(Input::GetMousePos().X, Input::GetMousePos().Y);

			//F_LOG_TRACE("Cursor Mouse PosX: {}, PosY: {} ", Input::GetMousePos().X, Input::GetMousePos().Y);

			//io.MouseDown[0] = Input::IsMouseButtonPressed(KeyNames::FL_MOUSE_BUTTON_2);
			//Key::GetState(KeyNames::FL_MOUSE_BUTTON_2)
			io.MouseDown[0] = Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_1);

			/*if(Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_1)) 
				F_LOG_TRACE("KeyNames::FL_MOUSE_BUTTON_1: {}", Input::IsMouseDown(KeyNames::FL_MOUSE_BUTTON_1));*/

			Renderer.DrawFrame();

            // Update timing
			Timing.Update();
            DeltaTime = Timing.GetDeltaTime();

			UpdateFps(Timing.GetTimeSinceStart());
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

	void Engine::UpdateFps(const float& totalTime)
	{
		m_fpsFrameCount++;

		float timeDiff =  totalTime - m_fpsTimeElapsed;
		if(timeDiff < 1.0f)
			return;

		mspf = 1000.0f / static_cast<float>(m_fpsFrameCount);

		m_fpsFrameCount = 0;
		m_fpsTimeElapsed += 1.0f;
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
		
		g_Registry.reset();

		// Cleanup any resources
		Input::Shutdown();
        ResourceManager::Get().Shutdown();
		Logger::Get().Shutdown();
        FlingConfig::Get().Shutdown();
		Timing::Get().Shutdown();
		Renderer::Get().Shutdown();
	}
}