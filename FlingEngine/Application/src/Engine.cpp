#include "ComponentTypeRegistry.h"
#include "DefaultPhysicsProvider.h"
#include "Ecs/RegisterEngineSystems.h"
#include "Engine.h"
#include "File.h"
#include "FlingConfig.h"
#include "FlingPaths.h"
#include "FlingWindow.h"
#include "Input/GlfwInput.h"
#include "Input/Input.h"
#include "Input/TickInput.h"
#include "Misc/CommandLine.h"
#include "Misc/CrashHandler.h"
#include "Physics.h"
#include "Random.h"
#include "RegisterGraphicsComponents.h"
#include "Threading/Thread.h"
#include "VulkanApp.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <random>
#include <thread>

namespace Fling
{
    ExitCode Engine::Startup(int argc, const char* argv[])
	{
		m_StartTime = std::chrono::steady_clock::now();

		// First, so that every later system sees this thread as GameThreadId
		Thread::InitGameThread("Game Thread");

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
		RegisterPhysicsComponents();

		// The game can bring its own physics provider. If it does not, use the one
		// that this build compiled (FLING_PHYSICS_PROVIDER).
		std::shared_ptr<PhysicsProvider> PhysicsProviderToUse = m_GameImpl->CreatePhysicsProvider();
		if (!PhysicsProviderToUse)
		{
			PhysicsProviderToUse = CreateDefaultPhysicsProvider();
		}
		// [Physics] WorkerThreads: threads that help with each physics step. 0 (or
		// missing) runs the step on the game thread. The result is the same either way.
		PhysicsSettings InitialPhysicsSettings;
		InitialPhysicsSettings.m_WorkerThreadCount =
			static_cast<uint32>(std::max(FlingConfig::GetInt("Physics", "WorkerThreads"), 0));
		F_LOG_TRACE("Physics provider: {} ({} worker threads)",
			PhysicsProviderToUse->GetName(), InitialPhysicsSettings.m_WorkerThreadCount);

		// The physics scene is in the registry context, so a level load (registry.reset) keeps it
		Physics::InitWorld(g_Registry, PhysicsProviderToUse, InitialPhysicsSettings);

		m_World = new World(g_Registry, m_GameImpl);
		m_GameImpl->m_OwningWorld = m_World;

		// A --null-render run has no real frame time, so it runs one fixed tick per frame.
		// Engine and module systems are added to m_World->GetSystems() here, before
		// World::Init adds the game's systems and builds the schedule.
		SystemRunnerConfig RunnerConfig;
		RunnerConfig.OneTickPerFrame = m_Options.NullRender;
		m_World->SetSystemRunnerConfig(RunnerConfig);
		RegisterEngineSystems(m_World->GetSystems());
		RegisterPhysicsSystems(m_World->GetSystems());

		// Each fixed tick takes the input messages that no earlier tick took, with its own
		// cursor on the input queue. UI capture (ImGui) gives the simulation neutral input.
		auto TickInputs = std::make_shared<TickInputBuilder>(Input::Get().GetHandler());
		m_World->SetTickInputSource([TickInputs](TickInput& t_Out)
		{
			const Input& GameInput = Input::Get();
			TickInputs->Build(t_Out, GameInput.IsKeyboardCaptured(), GameInput.IsMouseCaptured());
		});

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

	ExitCode Engine::Tick()
	{
		assert(m_World && m_GameImpl);		// We HAVE to have a world and a game

		Timing& Timing = Timing::Get();

		// Once the world is initialized it allows the users to add their own components!
		// A schedule with errors stops here: World::Init logged each error.
		const bool bWorldReady = m_World->Init(m_Options.Level);

		if (m_Options.DumpSchedule || m_Options.ValidateSchedule)
		{
			if (m_Options.DumpSchedule && m_World->GetSchedule())
			{
				std::fputs(m_World->GetSchedule()->Dump().c_str(), stdout);
			}
			if (!bWorldReady)
			{
				std::fputs(m_World->GetScheduleErrors().c_str(), stderr);
			}
			std::fflush(stdout);
			F_LOG_TRACE("System schedule {} (--dump-schedule / --validate-schedule)", bWorldReady ? "is valid" : "has errors");
			return bWorldReady ? ExitCode::Success : ExitCode::Failure;
		}

		if (!bWorldReady)
		{
			return ExitCode::Failure;
		}

        // The developer UI starts the game with its "Play" button. With no developer
        // UI (a game build, or --null-render), the game starts on the first tick.
        if (m_DebugUI)
        {
            // In editor play, a game that wants to quit (Escape) stops play, and
            // the editor stays open
            m_World->SetGameQuitStopsPlay(true);
        }
        else
        {
			m_World->RequestGameStart();
		}

		const float NullRenderDeltaTime = 1.0f / static_cast<float>(LaunchOptions::NullRenderTickRate);

		// While the window is in the background, a frame takes at least this long. The swap
		// chain does not wait for vsync, so with no cap a background game takes the whole GPU.
		constexpr std::chrono::steady_clock::duration BackgroundFrameTime =
			std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(1)) / 30;

		while(ShouldTick())
		{
			const std::chrono::steady_clock::time_point FrameStart = std::chrono::steady_clock::now();

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

			// Physics runs inside each fixed tick now: the Physics.Step system, in the
			// ResolveDeferred phase, after every system that requests a force.
			++m_TicksRun;

			Input::Get().EndTick();

			// Requests from systems that only the application can carry out
			for (const AppRequest Request : m_World->TakeAppRequests())
			{
				if (Request == AppRequest::ToggleCursor && !m_Options.NullRender)
				{
					FlingWindow* Window = VulkanApp::Get().GetCurrentWindow();
					if (Window)
					{
						Window->SetMouseVisible(!Window->GetMouseVisible());
					}
				}
			}

			if(m_World->ShouldQuit())
			{
				F_LOG_TRACE("World should quit! Exiting engine loop...");
				break;
			}

			if (!m_Options.NullRender)
			{
				VulkanApp::Get().Update(DeltaTime, g_Registry);

				const bool bFocused = VulkanApp::Get().GetCurrentWindow()->IsFocused();
				if (bFocused == m_FramesCapped)
				{
					m_FramesCapped = !bFocused;
					if (m_FramesCapped)
					{
						F_LOG_TRACE("Window in the background: frames capped at 30 a second");
					}
					else
					{
						F_LOG_TRACE("Window in front: frames uncapped");
					}
				}

				// The game ticks with the real delta time, so it runs at the same speed, only
				// in fewer and longer ticks
				if (m_FramesCapped)
				{
					std::this_thread::sleep_until(FrameStart + BackgroundFrameTime);
				}
			}

			Timing.UpdateFps();
		}
		return ExitCode::Success;
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

		Physics::ShutdownWorld(g_Registry);
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
