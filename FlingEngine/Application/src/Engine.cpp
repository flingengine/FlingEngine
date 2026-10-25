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
#include "Input/GlfwInput.h"
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

		if (m_Options.Unattended && !CrashHandler::IsAvailable())
		{
			F_LOG_WARN("--unattended: this build has no crash handler (WITH_CRASH_HANDLER_FLAG=OFF). A crash can open a dialog.");
		}

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
		Input::Get().SetLogLevel(m_Options.InputLog);
		Input::Get().Init();

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

#if WITH_INPUT_RECORDING
		// Start input replay and recording before the world exists. A file that cannot
		// be read then exits with code 3, and Shutdown has no world to stop.
		if (!m_Options.ReplayInput.empty())
		{
			const std::string ReplayPath = FlingPaths::ResolveProjectPath(m_Options.ReplayInput);
			std::string ReplayError;
			if (!Input::Get().GetPlayback().Start(ReplayPath, 0.0f, ReplayError))
			{
				F_LOG_ERROR("Cannot replay input: {}", ReplayError);
				return ExitCode::BadArguments;
			}
			const InputPlayback& Playback = Input::Get().GetPlayback();
			F_LOG_TRACE("Replaying input from {} ({} messages, {:.2f} s)", ReplayPath, Playback.GetMessageCount(), Playback.GetLength());
		}

		if (!m_Options.RecordInput.empty())
		{
			const std::string RecordPath = FlingPaths::ResolveProjectPath(m_Options.RecordInput);
			Input::Get().GetRecorder().Start(RecordPath, 0.0f, 0.0f);
			F_LOG_TRACE("Recording input to {}", RecordPath);
		}
#endif

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
				static_cast<PipelineFlags>(PipelineFlags::DEFERRED | PipelineFlags::CUBEMAP | PipelineFlags::IMGUI),
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
			// The window exists now. Its events are pumped by the GLFW input interface
			m_InputWindow = GetInputWindowHandle(VulkanApp::Get().GetCurrentWindow());
			m_GlfwInputId = Input::Get().AddInterface(CreateGlfwInputDeviceInterface());
			Input::Get().NotifyWindowCreated(m_InputWindow);
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

			// Poll pumps the platform events (GLFW) when there is a window, so it comes before
			// anything that reads the window state. Under --null-render there are no window
			// devices. DeltaTime moves the input recorder and playback clocks, so a
			// --null-render replay is the same every time.
			Input::Get().Poll(DeltaTime);

			// The game sees the input up to now, and the same input for the whole tick
			Input::Get().BeginTick();

			// World update will handle the starting, updating, and stopping of game logic
			m_World->Update(DeltaTime);
			++m_TicksRun;

			Input::Get().EndTick();

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
#if WITH_INPUT_RECORDING
			Input::Get().GetPlayback().Stop();
			InputRecorder& Recorder = Input::Get().GetRecorder();
			if (Recorder.IsActive())
			{
				const std::string RecordPath = Recorder.GetPath();
				if (Recorder.Stop())
				{
					F_LOG_TRACE("Saved input recording to {}", RecordPath);
				}
				else
				{
					F_LOG_ERROR("Cannot save input recording to {}: {}", RecordPath, Recorder.GetLastError());
				}
			}
#endif

			if (m_GlfwInputId.IsValid())
			{
				// The window is still alive here (VulkanApp::Shutdown is below)
				Input::Get().NotifyWindowDestroyed(m_InputWindow);
				Input::Get().RemoveInterface(m_GlfwInputId);
				m_GlfwInputId = InputInterfaceId{};
				m_InputWindow = WindowHandle{};
			}
			Input::Get().Shutdown();
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
