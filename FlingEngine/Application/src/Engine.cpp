#include "DefaultPhysicsProvider.h"
#include "Ecs/RegisterEngine.h"
#include "Engine.h"
#include "File.h"
#include "FlingConfig.h"
#include "FlingPaths.h"
#include "FlingWindow.h"
#include "Input/GlfwInput.h"
#include "Input/Input.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInputProvider.h"
#include "Localization.h"
#include "MathUtils.h"
#include "Misc/CommandLine.h"
#include "Misc/CrashHandler.h"
#include "Physics.h"
#include "PresentMode.h"
#include "Random.h"
#include "RegisterGraphics.h"
#include "Screenshot.h"
#include "Strings/String.h"
#include "Threading/Lane.h"
#include "Threading/Thread.h"
#include "VulkanApp.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>

namespace Fling
{
    Engine::~Engine()
    {
        // An exception out of Tick skips Shutdown: stop play first, so the game thread never
        // runs on the members below (the headless clock, the input) after they are gone
        if (m_World)
        {
            m_World->Shutdown();
        }
        if (m_HeadlessClock)
        {
            PlatformTime::UseVirtualClock(nullptr);
        }
    }

    ExitCode Engine::StartLocalization()
    {
        const String Folder = FlingConfig::GetString("Localization", "Folder", "Localization");
        const Name SourceLanguage(FlingConfig::GetString("Localization", "SourceLanguage", "en"));
        auto Loc =
            std::make_shared<Localization>(PathFromUtf8(FlingPaths::EngineAssetsDir() + "/" + Folder), SourceLanguage);

        Loc->LoadLanguage(SourceLanguage);
        const String Language =
            !m_Options.Language.empty()
                ? m_Options.Language
                : FlingConfig::GetString("Localization", "Language", String(SourceLanguage.ToView()));
        Loc->SetLanguage(Name(Language));
        F_LOG_TRACE("Language: {} (source {}), {}",
                    Loc->GetLanguage(),
                    SourceLanguage,
                    Loc->GetTextDirection() == TextDirection::RightToLeft ? "right to left" : "left to right");
        Localization::SetActive(Loc);

        if (!m_Options.LocImport.empty())
        {
            const String ImportPath = FlingPaths::ResolveProjectPath(m_Options.LocImport);
            std::ifstream File(PathFromUtf8(ImportPath), std::ios::binary);
            if (!File)
            {
                F_LOG_ERROR("Cannot read {}", ImportPath);
                return ExitCode::Failure;
            }
            const String Csv((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());

            // Load every language first, so the saved tables keep the entries that the
            // file does not have.
            for (const Name OnDisk : Loc->FindLanguagesOnDisk())
            {
                Loc->LoadLanguage(OnDisk);
            }

            String Error;
            std::vector<Name> Languages;
            if (!Loc->ImportCsv(Csv, Error, &Languages))
            {
                F_LOG_ERROR("Cannot import {}: {}", ImportPath, Error);
                return ExitCode::Failure;
            }
            for (const Name Imported : Languages)
            {
                if (!Loc->SaveLanguage(Imported, Error))
                {
                    F_LOG_ERROR("Cannot save language {}: {}", Imported, Error);
                    return ExitCode::Failure;
                }
                const size_t MissingCount = Loc->FindMissing(Imported).size();
                F_LOG_TRACE("Imported language {} ({} entries missing)", Imported, MissingCount);
            }
        }

        if (!m_Options.LocGather.empty())
        {
            const String Folder = FlingPaths::ResolveProjectPath(m_Options.LocGather);
            std::error_code Error;
            std::vector<GatheredEntry> Entries;
            for (auto It = std::filesystem::recursive_directory_iterator(PathFromUtf8(Folder), Error);
                 !Error && It != std::filesystem::recursive_directory_iterator();
                 It.increment(Error))
            {
                const std::filesystem::path Extension = It->path().extension();
                if (!It->is_regular_file() ||
                    (Extension != ".cpp" && Extension != ".h" && Extension != ".hpp" && Extension != ".inl"))
                {
                    continue;
                }
                std::ifstream File(It->path(), std::ios::binary);
                const String Code((std::istreambuf_iterator<char>(File)), std::istreambuf_iterator<char>());
                const String Location = PathToUtf8(std::filesystem::relative(It->path(), PathFromUtf8(Folder), Error));
                for (GatheredEntry& Entry : Localization::GatherFromCode(Code, Location))
                {
                    Entries.push_back(std::move(Entry));
                }
            }
            if (Error)
            {
                F_LOG_ERROR("Cannot search {}: {}", Folder, Error.message());
                return ExitCode::Failure;
            }

            const GatherResult Result = Loc->AddGathered(Entries);
            String SaveError;
            if (Result.Added > 0 && !Loc->SaveLanguage(SourceLanguage, SaveError))
            {
                F_LOG_ERROR("Cannot save language {}: {}", SourceLanguage, SaveError);
                return ExitCode::Failure;
            }
            F_LOG_TRACE("Gathered {} string table uses from {}: {} added, {} with different text, {} with no text",
                        Entries.size(),
                        Folder,
                        Result.Added,
                        Result.Mismatched,
                        Result.MissingText);
        }

        if (!m_Options.LocExport.empty())
        {
            for (const Name OnDisk : Loc->FindLanguagesOnDisk())
            {
                Loc->LoadLanguage(OnDisk);
            }

            const String ExportPath = FlingPaths::ResolveProjectPath(m_Options.LocExport);
            std::ofstream File(PathFromUtf8(ExportPath), std::ios::binary);
            File << Loc->ExportCsv();
            if (!File)
            {
                F_LOG_ERROR("Cannot write {}", ExportPath);
                return ExitCode::Failure;
            }
            F_LOG_TRACE("Exported string tables to {}", ExportPath);
        }

        return ExitCode::Success;
    }

    ExitCode Engine::Startup(int argc, const char* argv[])
    {
        m_StartTime = std::chrono::steady_clock::now();

        // First, so that every later system sees this thread as MainThreadId
        Thread::InitMainThread("Main Thread");

        if (const ExitCode Code = ReadLaunchOptions(argc, argv); Code != ExitCode::Success)
        {
            return Code;
        }
        // Before the systems start, so a run setting that is not valid stops the run with
        // nothing to shut down
        if (const ExitCode Code = ResolveSettings(); Code != ExitCode::Success)
        {
            return Code;
        }
        m_SystemsStarted = true;
        StartSystems();
        if (const ExitCode Code = StartLocalization(); Code != ExitCode::Success)
        {
            return Code;
        }
        if (const ExitCode Code = StartInputFiles(); Code != ExitCode::Success)
        {
            return Code;
        }
        MakeWorld();
        if (const ExitCode Code = WireWorld(); Code != ExitCode::Success)
        {
            return Code;
        }
        StartDebugUI();

        // Every value is loaded. From here on any thread may read the command line
        CommandLine::Get().Freeze();
        return ExitCode::Success;
    }

    ExitCode Engine::ReadLaunchOptions(int argc, const char* argv[])
    {
        const bool bSuccessfulCommandLineInit = CommandLine::Get().Init(argc, argv);

        // Read the launch flags before anything else starts: they decide the log
        // format, and whether a crash may open a dialog
        String LaunchOptionsError;
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
            F_LOG_WARN("--unattended: this build has no crash handler (WITH_CRASH_HANDLER_FLAG=OFF). A crash can open "
                       "a dialog.");
        }

        if (!bLaunchOptionsValid)
        {
            F_LOG_ERROR("Bad launch arguments: {}", LaunchOptionsError);
            return ExitCode::BadArguments;
        }

        // Log the seed, so that --seed can repeat a run that used a random one
        m_Seed = m_Options.HasSeed ? m_Options.Seed : Random::MakeEntropySeed();
        Random::Init(m_Seed);
        F_LOG_TRACE("Random seed: {}", m_Seed);
        return ExitCode::Success;
    }

    ExitCode Engine::ResolveSettings()
    {
        FlingConfig::Get().Init();
        const String EngineConfigPath = FlingPaths::EngineConfigDir() + "/EngineConf.ini";
        if (!FlingConfig::Get().LoadConfigFile(EngineConfigPath))
        {
            F_LOG_WARN("NO EngineConf.ini has been provided! This may result in unexpected behavior from Fling!");
        }

        // Let the command line consider the same ini's [ConsoleVariables] section as a
        // fallback, so systems can query CommandLine::GetValueAs for values that were
        // only set in the config file. Command line args passed directly still win.
        CommandLine::Get().LoadConfigFile(EngineConfigPath);

        // The run settings: the command line, then [Engine] in the config file, then the
        // defaults. Before the headless clock, which moves by the tick length. The frame
        // cap's default needs the window (MakeWorld).
        const RunSettingsConfig SettingsConfig =
            RunSettingsConfig::Read([](const char* t_Key) { return FlingConfig::GetString("Engine", t_Key, ""); });
        String SettingsError;
        if (!RunSettings::Resolve(m_Options, SettingsConfig, m_Settings, SettingsError))
        {
            F_LOG_ERROR("Bad run settings: {}", SettingsError);
            return ExitCode::BadArguments;
        }
        for (const String& Ignored : m_Settings.Ignored)
        {
            F_LOG_WARN("{}", Ignored);
        }
        return ExitCode::Success;
    }

    void Engine::StartSystems()
    {
        if (CommandLine::Get().GetValueAs<bool>("WaitForDebugger", false))
        {
            F_LOG_TRACE("-WaitForDebugger was set, breaking into the debugger now");
            F_DEBUG_BREAK();
        }

        ResourceManager::Get().Init();
        Timing::Get().Init();

        // A headless run (--null-render or --offscreen) has no real time: the platform clock
        // is virtual, and the loop moves it by a fixed step per frame. Install it before
        // input, so every input message time comes from it.
        if (m_Options.IsHeadless())
        {
            m_HeadlessClock = std::make_unique<PlatformTime::VirtualClock>(HeadlessFrameNs(0));
            PlatformTime::UseVirtualClock(m_HeadlessClock.get());
        }
        Input::Get().SetLogLevel(m_Options.InputLog);
        Input::Get().Init();

        F_LOG_TRACE("Fling Engine Sourcedir:  \t{}", Fling::FlingPaths::EngineSourceDir());
        F_LOG_TRACE("Fling Engine Assets dir: \t{}", Fling::FlingPaths::EngineAssetsDir());
        F_LOG_TRACE("Fling Engine Logs dir:   \t{}", Fling::FlingPaths::EngineLogDir());
        F_LOG_TRACE("Fling Engine Config dir: \t{}", Fling::FlingPaths::EngineConfigDir());

#ifdef FLING_SHIPPING
        F_LOG_TRACE("Fling Engine Config: Shipping");
#endif
    }

    ExitCode Engine::StartInputFiles()
    {
#if WITH_INPUT_RECORDING
        // A recording notes the simulation rate of its tick cuts, and a replay of cuts made
        // at another rate is refused (InputPlayback::SetTickRate)
        Input::Get().GetRecorder().SetTickRate(m_Settings.SimHz);
        Input::Get().GetPlayback().SetTickRate(m_Settings.SimHz);

        // Start input replay and recording before the world exists. A file that cannot
        // be read then exits with code 3, and Shutdown has no world to stop.
        if (!m_Options.ReplayInput.empty())
        {
            const String ReplayPath = FlingPaths::ResolveProjectPath(m_Options.ReplayInput);
            String ReplayError;
            if (!Input::Get().GetPlayback().Start(ReplayPath, 0.0f, ReplayError))
            {
                F_LOG_ERROR("Cannot replay input: {}", ReplayError);
                return ExitCode::BadArguments;
            }
            const InputPlayback& Playback = Input::Get().GetPlayback();
            F_LOG_TRACE("Replaying input from {} ({} messages, {:.2f} s)",
                        ReplayPath,
                        Playback.GetMessageCount(),
                        Playback.GetLength());
        }

        if (!m_Options.RecordInput.empty())
        {
            const String RecordPath = FlingPaths::ResolveProjectPath(m_Options.RecordInput);
            Input::Get().GetRecorder().Start(RecordPath, 0.0f, 0.0f);
            F_LOG_TRACE("Recording input to {}", RecordPath);
        }
#endif
        return ExitCode::Success;
    }

    void Engine::MakeWorld()
    {
        // The world owns the entity storage. The renderer and physics keep a reference
        // to it, so make the world first, and destroy it after them.
        m_World = std::make_shared<World>(m_GameImpl);
        m_GameImpl->m_OwningWorld = m_World;
        EntityStorage& Storage = m_World->GetStorage();

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
            RenderSurfaceDesc Surface;
            Surface.Offscreen = m_Options.Offscreen;
            Surface.Width = m_Options.ResolutionWidth;
            Surface.Height = m_Options.ResolutionHeight;
            VulkanApp::Get().SetRenderSurface(Surface);

            uint32 Pipelines = PipelineFlags::DEFERRED | PipelineFlags::CUBEMAP;
            if (m_Options.Offscreen)
            {
                // The developer UI is ImGui, which reads its input from a window: offscreen there
                // is none, so the frames show the game only
                F_LOG_TRACE("Fling render: offscreen (--offscreen), no window");
                m_DebugUI.reset();
            }
            else
            {
                Pipelines |= PipelineFlags::IMGUI;
            }
            VulkanApp::Get().SetVSync(m_Settings.VSync);
            VulkanApp::Get().Init(static_cast<PipelineFlags>(Pipelines), Storage, m_DebugUI);
            F_LOG_TRACE("Render size: {}x{}", VulkanApp::Get().GetFrameWidth(), VulkanApp::Get().GetFrameHeight());
        }

        // The frame cap's default is the refresh rate of the window's monitor
        m_Settings.ResolveFrameCap(WindowMonitorHz());
        F_LOG_TRACE("Run settings: {}", m_Settings.Describe());

        // Each linked module adds its component types and systems. World::Init adds the
        // game's, then builds the type registry and the schedule.
        RegisterEngine(m_World->GetModules());
        RegisterGraphics(m_World->GetModules());
        RegisterPhysics(m_World->GetModules());

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
                    PhysicsProviderToUse->GetName(),
                    InitialPhysicsSettings.m_WorkerThreadCount);

        // The physics scene is in the registry context, so a level load (registry.reset) keeps it
        Physics::InitWorld(Storage, PhysicsProviderToUse, InitialPhysicsSettings);
    }

    ExitCode Engine::WireWorld()
    {
        // The ticks that are due come from the platform clock. A --null-render run reads
        // the virtual clock, which moves 1/N of a tick per frame (--frames-per-tick N), and
        // starts the timeline so tick 1 runs on the first frame, then one tick every N frames.
        // The rate changes only through a SetRate clock command (RunControls::RequestSimHz).
        SystemRunnerConfig RunnerConfig;
        RunnerConfig.SimHz = m_Settings.SimHz;
        m_World->SetSystemRunnerConfig(RunnerConfig);
        m_World->SetTickClock([]() { return PlatformTime::NowNanoseconds(); });
        // The per-tick state hash costs about 0.7 ms a tick in a Debug Sandbox (754
        // transforms): on for headless runs and --dump-ticks, off for a windowed game.
        m_World->SetStateHashEnabled(m_Options.IsHeadless() || !m_Options.DumpTicks.empty() ||
                                     !m_Options.DumpState.empty());
        m_World->SetInterpolation(m_Settings.Interpolation);
        m_Controls = std::make_unique<RunControls>(m_Settings, *m_World);
        UpdateFrameLimiter();
        if (m_Options.IsHeadless())
        {
            m_World->SetTickTimelineStart(HeadlessFrameNs(1) - TickNs());
            m_World->SetTickClockOnlyMovesWithFrames(true);
        }

        SetUpExecMode();

        // Each fixed tick takes, in sequence order, the input messages that no earlier tick
        // took and whose time is before the end of its window. A replay of a file with
        // per-tick cuts takes exactly the recorded cut instead, so it gets the same input
        // per tick at any frame rate. A recording notes each tick's cut. UI capture (ImGui)
        // comes from the input stream. The main thread polls (it is the input writer) in
        // every exec mode; World::MakeTickInputConfig says how a game thread waits for it: a
        // headless run or a replay waits for every message of the window (the cuts are the
        // same as in Direct), live windowed play does not wait.
        const int64 WatermarkWaitNs = (m_Options.IsHeadless() || !m_Options.ReplayInput.empty()) ? -1 : 0;
        auto TickInputs =
            std::make_shared<TickInputProvider>(Input::Get(), m_World->MakeTickInputConfig(WatermarkWaitNs));
        m_World->SetTickInputSource([TickInputs](TickInput& t_Out, const TickWindow& t_Window)
                                    { TickInputs->Build(t_Out, t_Window); });

        if (!m_Options.DumpTicks.empty())
        {
            const String DumpPath = FlingPaths::ResolveProjectPath(m_Options.DumpTicks);
            m_DumpTicksFile =
                std::make_unique<std::ofstream>(PathFromUtf8(DumpPath), std::ios::binary | std::ios::trunc);
            if (!*m_DumpTicksFile)
            {
                F_LOG_ERROR("Cannot write the tick dump {}", DumpPath);
                return ExitCode::BadArguments;
            }
            F_LOG_TRACE("Writing one line per tick to {} (--dump-ticks)", DumpPath);
            m_World->KeepTickRecords();
        }

        // --dump-state and --screenshot: at the listed ticks, or the last tick of the run
        if (!m_Options.DumpState.empty())
        {
            m_PendingStateDumps =
                m_Options.DumpStateTicks.empty() ? std::vector<uint64>{LastTickOfRun()} : m_Options.DumpStateTicks;
            m_World->SetStateDumpTicks(m_PendingStateDumps);
            F_LOG_TRACE("Writing the state at {} tick(s) to {} (--dump-state)",
                        m_PendingStateDumps.size(),
                        FlingPaths::ResolveProjectPath(m_Options.DumpState));
        }
        if (!m_Options.Screenshot.empty())
        {
            m_PendingScreenshots =
                m_Options.ScreenshotTicks.empty() ? std::vector<uint64>{LastTickOfRun()} : m_Options.ScreenshotTicks;
            F_LOG_TRACE("Saving the frames of {} tick(s) to {} (--screenshot)",
                        m_PendingScreenshots.size(),
                        FlingPaths::ResolveProjectPath(m_Options.Screenshot));
        }

        // The runner fills PendingInput for the frame side with the messages after the cut of the tick
        // it shows (RealFrameTime::Latest: the packet in the mirror in Inline and Threaded
        // play), without consuming them. UI capture comes from the stream, so it drops the
        // same messages as the tick.
        auto PendingInputs = std::make_shared<PendingInputReader>(Input::Get().GetHandler());
        m_World->SetPendingInputSource([PendingInputs](const uint64 t_AfterSequence, InputSlice& t_Out)
                                       { PendingInputs->Read(t_AfterSequence, t_Out); });

        // A second poll after the fixed ticks, right before the frame systems, so input that
        // came in while the ticks ran is pending for this frame (Input::PollLate).
        m_World->SetLateInputPoll([]() { Input::Get().PollLate(); });
        return ExitCode::Success;
    }

    void Engine::StartDebugUI()
    {
        // Before World::Init() seals the component type registry, so developer UI
        // (the editor) can still attach to component types
        if (m_DebugUI)
        {
            m_DebugUI->Init(m_World->GetStorage(), m_World, m_GameImpl, *m_Controls);
            F_LOG_TRACE("Fling debug UI: Enabled");
        }
        else
        {
            F_LOG_TRACE("Fling debug UI: Disabled");
        }

        if (!m_Options.IsHeadless())
        {
            // The window exists now. Its events are pumped by the GLFW input interface
            m_InputWindow = GetInputWindowHandle(VulkanApp::Get().GetCurrentWindow());
            m_GlfwInputId = Input::Get().AddInterface(CreateGlfwInputDeviceInterface());
            Input::Get().NotifyWindowCreated(m_InputWindow);
        }
    }

    ExitCode Engine::RunFrames()
    {
        assert(m_World && m_GameImpl); // We HAVE to have a world and a game

        // This thread runs the frame side; in threaded play the game thread owns the simulation storage
        const LaneCheck::Scope MainLane(Lane::Frame);

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
            F_LOG_TRACE("System schedule {} (--dump-schedule / --validate-schedule)",
                        bWorldReady ? "is valid" : "has errors");
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

            // --play starts play at once, as if the Play button was pressed (windowed tests)
            if (CommandLine::Get().GetValueAs<bool>("play", false))
            {
                m_World->RequestGameStart();
            }
        }
        else
        {
            m_World->RequestGameStart();
        }

        // With --frames-per-tick N, a frame is 1/N of a tick, so the input playback clock
        // sends the messages of a tick over its N frames
        const float NullRenderDeltaTime = 1.0f / static_cast<float>(m_Settings.SimHz * m_Options.FramesPerTick);

        const std::chrono::steady_clock::time_point LoopStart = std::chrono::steady_clock::now();
        while (ShouldTick())
        {
            Timing.Update();

            // Under --null-render each frame has the same delta and never waits on real time
            // for its ticks, so a run is as fast as the CPU allows
            const float DeltaTime = m_Options.IsHeadless() ? NullRenderDeltaTime : Timing.GetDeltaTime();
            Stats::Frames::FrameStats(DeltaTime);

            PumpInput(DeltaTime);
            m_Controls->OnFrame(PlatformTime::NowNanoseconds());

            // The play requests, this frame's ticks (the game thread runs them itself in
            // threaded play), the ticks' outbox, then the frame systems on what the world shows
            m_World->Update(DeltaTime);
            DumpNewTicks();
            WriteStateDumps();

            HandleAppRequests();
            if (m_World->ShouldQuit())
            {
                F_LOG_TRACE("World should quit! Exiting engine loop...");
                break;
            }

            if (m_Options.NullRender)
            {
                // No renderer to serve a Screenshot::Request: it fails now, so its caller hears back
                Screenshot::FailPending("there is no renderer (--null-render); use --offscreen");
                // No cap unless a setting asks for one (RunSettings::ResolveFrameCap), so tests stay fast
                m_FrameLimiter.WaitForNextFrame();
            }
            else
            {
                // A window, or offscreen images (--offscreen). The present saves the screenshots
                // that are waiting (Screenshot::Request), from --screenshot or anywhere else
                RequestScreenshots();
                Present(DeltaTime);
            }
            Timing.UpdateFps();
        }

        // The measured frame time, to check the frame cap (RunSettings::MaxFps)
        const double LoopSeconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - LoopStart).count();
        const double AverageFrameMs = m_FramesRun > 0 ? LoopSeconds * 1000.0 / static_cast<double>(m_FramesRun) : 0.0;
        F_LOG_TRACE("Run ({}): {} frames on the main thread, {} ticks, average frame {:.3f} ms ({:.1f} fps)",
                    ExecModeName(m_Settings.Exec),
                    m_FramesRun,
                    m_World->GetTickCount(),
                    AverageFrameMs,
                    AverageFrameMs > 0.0 ? 1000.0 / AverageFrameMs : 0.0);
        // The screenshots that frames copied are written on a thread: wait for them, so the checks
        // below see every result
        if (!m_Options.NullRender)
        {
            VulkanApp::Get().FinishScreenshots();
        }
        // A request that no frame served (the loop ended first) fails, so its caller hears back
        Screenshot::FailPending("the run ended before the next frame was drawn");

        // A screenshot that was asked for and not made fails the run, so a script does not read an old file
        bool bCapturesOk = m_ScreenshotsFailed == 0;
        for (const uint64 Tick : m_PendingScreenshots)
        {
            F_LOG_ERROR(
                "--screenshot: no frame showed tick {} (the run showed up to tick {})", Tick, m_World->GetShownTick());
            bCapturesOk = false;
        }
        if (!bCapturesOk)
        {
            return ExitCode::Failure;
        }
#if WITH_INPUT_RECORDING
        if (Input::Get().GetPlayback().HasOverrun())
        {
            F_LOG_ERROR("The input replay failed: more messages were sent than the input queue holds before a "
                        "tick took them, so a tick lost input");
            return ExitCode::Failure;
        }
#endif
        return ExitCode::Success;
    }

    void Engine::PumpInput(const float t_DeltaTime)
    {
        // Poll pumps the platform events (GLFW) when there is a window, so it comes before
        // anything that reads the window state. Under --null-render there are no window
        // devices. The delta moves the input recorder and playback clocks, so a
        // --null-render replay is the same every time.
        Input::Get().Poll(t_DeltaTime);

        // Headless: this frame's poll was at HeadlessFrameNs(f); its ticks are due at the end
        // of the frame, HeadlessFrameNs(f + 1). The late poll and the next frame's poll get
        // that time too.
        if (m_HeadlessClock)
        {
            m_HeadlessClock->Set(HeadlessFrameNs(m_FramesRun + 1));
        }
        ++m_FramesRun;
    }

    void Engine::HandleAppRequests()
    {
        for (const AppRequest Request : m_World->TakeAppRequests())
        {
            if (Request == AppRequest::ToggleCursor && !m_Options.IsHeadless())
            {
                FlingWindow* Window = VulkanApp::Get().GetCurrentWindow();
                if (Window)
                {
                    Window->SetMouseVisible(!Window->GetMouseVisible());
                }
            }
        }
    }

    void Engine::Present(const float t_DeltaTime)
    {
        // During Inline or Threaded play the renderer reads the mirror. A VSync change makes
        // the swap chain again after this frame's present.
        VulkanApp::Get().SetVSync(m_Controls->VSync());
        VulkanApp::Get().Update(t_DeltaTime, m_World->GetPresentedStorage());

        // Offscreen there is no window: no focus, no monitor, and no frame cap (headless)
        if (const FlingWindow* Window = VulkanApp::Get().GetCurrentWindow())
        {
            const bool bInBackground = !Window->IsFocused();
            if (bInBackground != m_Controls->InBackground())
            {
                m_Controls->SetInBackground(bInBackground);
                F_LOG_TRACE("Window in the {}", bInBackground ? "background" : "front");
            }
            // The window can move to another monitor; a frame cap that came from the monitor follows
            const int64 NowNs = PlatformTime::NowNanoseconds();
            if (NowNs >= m_NextMonitorCheckNs)
            {
                m_NextMonitorCheckNs = NowNs + MonitorCheckIntervalNs;
                m_Controls->SetMonitorHz(WindowMonitorHz());
            }
            UpdateFrameLimiter();
        }

        // The game ticks on its own clock, so it runs at the same speed, only shown less often
        m_FrameLimiter.WaitForNextFrame();
    }

    std::optional<uint32> Engine::WindowMonitorHz() const
    {
        const FlingWindow* Window = m_Options.IsHeadless() ? nullptr : VulkanApp::Get().GetCurrentWindow();
        return Window != nullptr ? std::optional<uint32>(Window->GetRefreshRate()) : std::nullopt;
    }

    void Engine::UpdateFrameLimiter()
    {
        // Headless there is no swap chain and no refresh rate, so the cap applies as it is
        const bool bDisplayPacesFrames =
            !m_Options.IsHeadless() && DisplayPacesFrames(VulkanApp::Get().GetPresentMode());
        const uint32 LimiterFps = m_Controls->LimiterFps(bDisplayPacesFrames);
        if (LimiterFps != m_FrameLimiter.GetMaxFps())
        {
            m_FrameLimiter.SetMaxFps(LimiterFps);
            F_LOG_TRACE("Frame limiter: {} (cap {} fps, present mode {})",
                        m_Controls->DescribeLimiter(bDisplayPacesFrames),
                        m_Controls->MaxFps(),
                        m_Options.IsHeadless() ? "none" : PresentModeName(VulkanApp::Get().GetPresentMode()));
        }
    }

    uint64 Engine::LastTickOfRun() const
    {
        const uint64 PerTick = Math::Max<uint64>(m_Options.FramesPerTick, 1);
        return (static_cast<uint64>(m_Options.Frames) + PerTick - 1) / PerTick;
    }

    void Engine::SetUpExecMode()
    {
        m_World->SetExecMode(m_Settings.Exec);
        m_World->SetLastTick(LastTickOfRun());
        if (m_Settings.Exec != ExecMode::Threaded)
        {
            return;
        }

        // Threaded: the main thread polls, the game thread reads the queue. A headless clock
        // moves only when the main thread steps it, so a headless game thread runs each tick
        // as soon as it is due.
        Input::Get().SetWriterLane(Lane::Frame);
        GameLaneDesc Desc;
        Desc.Pacing = m_Options.IsHeadless() ? GameLanePacing::AsFastAsPossible : GameLanePacing::RealTime;
        Desc.JitterSeed = m_Options.JitterSeed;
        Desc.JitterMaxMicroseconds = m_Options.JitterMaxMicroseconds;
        m_World->SetGameLaneDesc(Desc);

        // The frame side's jitter has its own stream
        m_World->SetFrameJitter(ThreadJitter(m_Options.JitterSeed + 1, m_Options.JitterMaxMicroseconds));
    }

    int64 Engine::TickNs() const
    {
        return SimRate::StepNs(m_Settings.SimHz);
    }

    int64 Engine::HeadlessFrameNs(const uint64 t_Frame) const
    {
        // Counted from the frame number, never summed, so it does not drift. It starts one
        // tick after 0, so the first tick's window starts after 0.
        const uint64 PerTick = std::max<uint32>(m_Options.FramesPerTick, 1);
        return TickNs() + static_cast<int64>((t_Frame * static_cast<uint64>(TickNs())) / PerTick);
    }

    void Engine::DumpNewTicks()
    {
        if (!m_DumpTicksFile)
        {
            return;
        }
        char Line[224];
        for (const TickRecord& Record : m_World->TakeNewTickRecords())
        {
            std::snprintf(Line,
                          sizeof(Line),
                          "tick=%llu dt=%lld elapsed=%lld cut=%llu samples=%u keys=%u hash=%016llx%s\n",
                          static_cast<unsigned long long>(Record.Stamp.Tick),
                          static_cast<long long>(Record.Stamp.FixedDeltaNs),
                          static_cast<long long>(Record.Stamp.ElapsedNs),
                          static_cast<unsigned long long>(Record.Stamp.Cut.LastSequence),
                          Record.MouseSamples,
                          Record.KeyEvents,
                          static_cast<unsigned long long>(Record.Stamp.StateHash),
                          Record.InputOverran ? " input-overran" : "");
            *m_DumpTicksFile << Line;
        }
        m_DumpTicksFile->flush();
    }

    void Engine::WriteStateDumps()
    {
        if (!m_World || m_Options.DumpState.empty())
        {
            return;
        }
        const bool bManyTicks = m_Options.DumpStateTicks.size() > 1;
        for (const World::StateDump& Dump : m_World->TakeStateDumps())
        {
            std::erase(m_PendingStateDumps, Dump.Tick);
            const String Path =
                FlingPaths::ResolveProjectPath(CaptureFilePath(m_Options.DumpState, Dump.Tick, bManyTicks));
            String WriteError;
            if (!FlingPaths::SaveFile(Path, Dump.Doc.Dump(2) + '\n', WriteError))
            {
                F_LOG_ERROR("--dump-state: {}", WriteError);
                ++m_StateDumpsFailed;
                continue;
            }
            F_LOG_TRACE("Wrote the state of tick {} ({} entities, state hash {}) to {}",
                        Dump.Tick,
                        Dump.Doc.GetUInt64("entityCount"),
                        Dump.Doc.GetString("stateHash"),
                        Path);
        }
    }

    void Engine::RequestScreenshots()
    {
        if (m_PendingScreenshots.empty())
        {
            return;
        }
        // The frame that first shows each tick. With threaded play the frame side can show a
        // later tick than the one asked for; the log line says which tick the file shows.
        const uint64 Shown = m_World->GetShownTick();
        const bool bManyTicks = m_Options.ScreenshotTicks.size() > 1;
        while (!m_PendingScreenshots.empty() && m_PendingScreenshots.front() <= Shown)
        {
            const uint64 Tick = m_PendingScreenshots.front();
            m_PendingScreenshots.erase(m_PendingScreenshots.begin());
            // The renderer serves it at this frame's present; the callback runs on this thread
            Screenshot::Request(CaptureFilePath(m_Options.Screenshot, Tick, bManyTicks),
                                [this, Tick, Shown](const ScreenshotResult& t_Result)
                                {
                                    if (!t_Result.Ok)
                                    {
                                        ++m_ScreenshotsFailed;
                                        return;
                                    }
                                    F_LOG_TRACE("Saved the frame of tick {} (it shows tick {}, {}x{}) to {}",
                                                Tick,
                                                Shown,
                                                t_Result.Width,
                                                t_Result.Height,
                                                t_Result.Path);
                                });
        }
    }

    bool Engine::ShouldTick() const
    {
        // --frames: the run ends after N frames, once the frame side shows the last tick (the
        // world runs no tick after it). With the ticks on a game thread the two are not in step.
        if (m_Options.Frames > 0 && m_FramesRun >= m_Options.Frames && m_World->GetShownTick() >= LastTickOfRun())
        {
            F_LOG_TRACE("Ran {} frames and showed tick {} (--frames). Exiting engine loop...",
                        m_FramesRun,
                        m_World->GetShownTick());
            return false;
        }

        if (!m_Options.IsHeadless() && VulkanApp::Get().GetCurrentWindow()->ShouldClose())
        {
            return false;
        }

        return true;
    }

    ExitCode Engine::Shutdown(ExitCode t_Code)
    {
        // Cleanup game play stuff. The game thread joins first.
        if (m_World)
        {
            m_World->Shutdown();
            m_TicksAtExit = m_World->GetTickCount();

            // No frame is drawn from here on: a screenshot request still waiting fails, so its caller hears back
            Screenshot::FailPending("the engine shut down before the next frame was drawn");

            // The game thread has joined: a dump that it made after the last frame is here now
            WriteStateDumps();
            if (!m_Options.DumpState.empty() && t_Code == ExitCode::Success)
            {
                bool bDumpsOk = m_StateDumpsFailed == 0;
                for (const uint64 Tick : m_PendingStateDumps)
                {
                    F_LOG_ERROR("--dump-state: tick {} did not run (the run ran {} ticks)", Tick, m_TicksAtExit);
                    bDumpsOk = false;
                }
                if (!bDumpsOk)
                {
                    t_Code = ExitCode::Failure;
                }
            }
        }

        // The world still holds the game until the world is destroyed below
        m_GameImpl.reset();

        // Cleanup any resources
        if (m_SystemsStarted)
        {
#if WITH_INPUT_RECORDING
            Input::Get().GetPlayback().Stop();
            InputRecorder& Recorder = Input::Get().GetRecorder();
            if (Recorder.IsActive())
            {
                const String RecordPath = Recorder.GetPath();
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
            PlatformTime::UseVirtualClock(nullptr);
            Localization::SetActive(nullptr);
            ResourceManager::Get().Shutdown();
            FlingConfig::Get().Shutdown();
            Timing::Get().Shutdown();

            if (!m_Options.NullRender && m_World)
            {
                VulkanApp::Get().ShutdownRenderer();
            }
        }

        m_Controls.reset();
        if (m_World)
        {
            Physics::ShutdownWorld(m_World->GetStorage());
            m_World->GetStorage().Clear();
            m_World.reset();
        }

        PrintExitSummary(t_Code);

        // Last, so that every system above can still log while it shuts down
        Logger::Get().Shutdown();

        return t_Code;
    }

    void Engine::PrintExitSummary(ExitCode t_Code) const
    {
        const double WallSeconds =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - m_StartTime).count();
        const char* Render = m_Options.NullRender ? "null" : (m_Options.Offscreen ? "offscreen" : "vulkan");

        if (m_Options.Format == LogFormat::Json)
        {
            std::printf(
                "{\"event\":\"exit\",\"code\":%d,\"ticks\":%llu,\"seed\":%u,\"render\":\"%s\",\"wallSeconds\":%.3f}\n",
                ToInt(t_Code),
                static_cast<unsigned long long>(m_TicksAtExit),
                m_Seed,
                Render,
                WallSeconds);
        }
        else
        {
            std::printf("Fling exit: code=%d ticks=%llu seed=%u render=%s wall=%.3fs\n",
                        ToInt(t_Code),
                        static_cast<unsigned long long>(m_TicksAtExit),
                        m_Seed,
                        Render,
                        WallSeconds);
        }

        std::fflush(stdout);
    }
} // namespace Fling
