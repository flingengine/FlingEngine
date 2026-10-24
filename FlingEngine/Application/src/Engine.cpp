#include "Engine.h"
#include <cstdint>
#include <cstdio>
#include <random>
#include "File.h"
#include "VulkanApp.h"
#include "Misc/CommandLine.h"
#include "Misc/CrashHandler.h"
#include "ComponentTypeRegistry.h"
#include "RegisterGraphicsComponents.h"
#include "FlingPaths.h"
#include "FlingWindow.h"
#include "Input/Input.h"
#include "Random.h"

namespace Fling
{
    ExitCode Engine::Startup(int argc, const char* argv[])
	{
		m_StartTime = std::chrono::steady_clock::now();

    	// Initalize the command line
		const bool bSuccessfulCommandLineInit = CommandLine::Get().Init(argc, argv);

		// Read the launch flags before anything else starts: they decide the log
		// format, and whether a crash may open a dialog
		std::string LaunchOptionsError;
		const bool bLaunchOptionsValid = LaunchOptions::Parse(CommandLine::Get(), m_Options, LaunchOptionsError);

		if (m_Options.Unattended)
		{
			CrashHandler::InstallUnattended();
		}

		Logger::SetFormat(m_Options.Format);
		Logger::Get().Init();

		F_LOG_TRACE("Command line initaliziation: {}\t", bSuccessfulCommandLineInit ? "successful" : "failed");
		F_LOG_TRACE("Command line args: {}\t", CommandLine::Get().GetCommandLineData());

		if (!bLaunchOptionsValid)
		{
			F_LOG_ERROR("Bad launch arguments: {}", LaunchOptionsError);
			return ExitCode::BadArguments;
		}

		// Log the seed, so that --seed can repeat a run that used a random one
		m_Seed = m_Options.HasSeed ? m_Options.Seed : std::random_device{}();
		Random::Init(m_Seed);
		F_LOG_TRACE("Random seed: {}", m_Seed);

		m_SystemsStarted = true;

		FlingConfig::Get().Init();

		if (CommandLine::Get().GetValueAs<bool>("WaitForDebugger", false))
		{
			F_LOG_TRACE("-WaitForDebugger was set, breaking into the debugger now");
			F_DEBUG_BREAK();
		}

		ResourceManager::Get().Init();
		Timing::Get().Init();
		Input::Init();

        F_LOG_TRACE("Fling Engine Sourcedir:  \t{}", Fling::FlingPaths::EngineSourceDir());
        F_LOG_TRACE("Fling Engine Assets dir: \t{}", Fling::FlingPaths::EngineAssetsDir());
        F_LOG_TRACE("Fling Engine Logs dir:   \t{}", Fling::FlingPaths::EngineLogDir());
        F_LOG_TRACE("Fling Engine Config dir: \t{}", Fling::FlingPaths::EngineConfigDir());

#ifdef FLING_SHIPPING
		F_LOG_TRACE("Fling Engine Config: Shipping");
#endif

        // Load command line args and any ini files
        const std::string EngineConfigPath = FlingPaths::EngineConfigDir() + "/EngineConf.ini";
        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile(EngineConfigPath);

		if (!ConfigLoaded)
		{
			F_LOG_WARN("NO EngineConf.ini has been provided! This may result in unexpected behavior from Fling!");
		}

		// Let the command line consider the same ini's [ConsoleVariables] section as a
		// fallback, so systems can query CommandLine::GetValueAs for values that were
		// only set in the config file. Command line args passed directly still win.
		CommandLine::Get().LoadConfigFile(EngineConfigPath);

		if (m_Options.NullRender)
		{
			// No window, no swapchain, and no GPU device. Graphics resources keep their
			// CPU data only (see VulkanApp::HasDevice). The developer UI draws with
			// ImGui in the renderer, so it cannot run either.
			F_LOG_TRACE("Fling render: null (--null-render)");
			m_DebugUI.reset();
		}
		else
		{
			VulkanApp::Get().Init(
				static_cast<PipelineFlags>(PipelineFlags::DEFERRED | PipelineFlags::IMGUI),
				g_Registry,
				m_DebugUI
			);
		}

		RegisterEngineComponents();
		RegisterGraphicsComponents();

		m_World = new World(g_Registry, m_GameImpl);
		m_GameImpl->m_OwningWorld = m_World;

		// Before World::Init() seals the component type registry, so developer UI
		// (the editor) can still attach to component types
		if (m_DebugUI)
		{
			m_DebugUI->Init(g_Registry, m_World, m_GameImpl);
			F_LOG_TRACE("Fling debug UI: Enabled");
		}
		else
		{
			F_LOG_TRACE("Fling debug UI: Disabled");
		}

		if (!m_Options.NullRender)
		{
			Input::PreUpdate();
		}

		return ExitCode::Success;
	}

	void Engine::Tick()
	{
		assert(m_World && m_GameImpl);		// We HAVE to have a world and a game

		Timing& Timing = Timing::Get();

		// Once the world is initialized it allows the users to add their own components!
		m_World->Init();

		// The developer UI starts the game with its "Play" button. With no developer
		// UI (a game build, or --null-render), the game starts now.
		if (!m_DebugUI)
		{
			m_World->RequestGameStart();
		}

		const float NullRenderDeltaTime = 1.0f / static_cast<float>(LaunchOptions::NullRenderTickRate);

		while(ShouldTick())
		{
            // Update timing
            Timing.Update();

			// Under --null-render each tick has the same delta and the loop never waits
			// on real time, so a run is as fast as the CPU allows
            const float DeltaTime = m_Options.NullRender ? NullRenderDeltaTime : Timing.GetDeltaTime();

			// Update FPS Counter
            Stats::Frames::TickStats(DeltaTime);

			// There are no input devices without a window
			if (!m_Options.NullRender)
			{
				Input::Poll();
			}

			// World update will handle the starting, updating, and stopping of game logic
			m_World->Update(DeltaTime);
			++m_TicksRun;

			if(m_World->ShouldQuit())
			{
				F_LOG_TRACE("World should quit! Exiting engine loop...");
				break;
			}

			if (!m_Options.NullRender)
			{
				VulkanApp::Get().Update(DeltaTime, g_Registry);
			}

			Timing.UpdateFps();
		}
	}

	bool Engine::ShouldTick() const
	{
		if (m_Options.Frames > 0 && m_TicksRun >= m_Options.Frames)
		{
			F_LOG_TRACE("Ran {} ticks (--frames). Exiting engine loop...", m_TicksRun);
			return false;
		}

		if (!m_Options.NullRender && VulkanApp::Get().GetCurrentWindow()->ShouldClose())
		{
			return false;
		}

		return true;
	}

	ExitCode Engine::Shutdown(ExitCode t_Code)
	{
		// Cleanup game play stuff
		if(m_World)
		{
			m_World->Shutdown();
		}

    	delete m_GameImpl;
    	m_GameImpl = nullptr;

    	delete m_World;
    	m_World = nullptr;

		// Cleanup any resources
		if (m_SystemsStarted)
		{
			Input::Shutdown();
			ResourceManager::Get().Shutdown();
			FlingConfig::Get().Shutdown();
			Timing::Get().Shutdown();

			if (!m_Options.NullRender)
			{
				VulkanApp::Get().Shutdown(g_Registry);
			}
		}

		g_Registry.reset();

		PrintExitSummary(t_Code);

		// Last, so that every system above can still log while it shuts down
		Logger::Get().Shutdown();

		return t_Code;
	}

	void Engine::PrintExitSummary(ExitCode t_Code) const
	{
		const double WallSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartTime).count();
		const char* Render = m_Options.NullRender ? "null" : "vulkan";

		if (m_Options.Format == LogFormat::Json)
		{
			std::printf(
				"{\"event\":\"exit\",\"code\":%d,\"ticks\":%llu,\"seed\":%u,\"render\":\"%s\",\"wallSeconds\":%.3f}\n",
				ToInt(t_Code), static_cast<unsigned long long>(m_TicksRun), m_Seed, Render, WallSeconds
			);
		}
		else
		{
			std::printf(
				"Fling exit: code=%d ticks=%llu seed=%u render=%s wall=%.3fs\n",
				ToInt(t_Code), static_cast<unsigned long long>(m_TicksRun), m_Seed, Render, WallSeconds
			);
		}

		std::fflush(stdout);
	}
}	// namespace Fling
