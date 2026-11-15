#pragma once

#include "ExitCode.h"
#include "FlingConfig.h"
#include "Game.h"
#include "IDebugUI.h"
#include "Input/InputDeviceTypes.h"
#include "Input/TickInputProvider.h"
#include "LaunchOptions.h"
#include "Logger.h"
#include "MovingAverage.hpp"
#include "Platform.h"
#include "PlatformTime.h"
#include "ResourceManager.h"
#include "RunControls.h"
#include "RunSettings.h"
#include "Stats.h"
#include "Threading/FrameLimiter.h"
#include "Timing.h"
#include "World.h"

#include <chrono>
#include <csignal>
#include <fstream>
#include <memory>
#include <optional>
#include <type_traits>

/**
 * Triggers a breakpoint at the call site, halting execution if a debugger is attached.
 * Works across compilers/platforms so callers don't need their own #ifdef ladder.
 */
#if defined(_MSC_VER)
#define F_DEBUG_BREAK() __debugbreak()
#elif defined(__clang__)
#define F_DEBUG_BREAK() __builtin_debugtrap()
#elif defined(__MINGW32__)
// MinGW-w64 targets Windows, which has no SIGTRAP to raise()
#define F_DEBUG_BREAK() asm("int $3")
#elif defined(__GNUC__)
#define F_DEBUG_BREAK() raise(SIGTRAP)
#else
#define F_DEBUG_BREAK()
#endif

namespace Fling
{
    /**
     * Core engine class of Fling. This is where the core update loop lives
     * along with all startup/shutdown ordering.
     *
     * The loop does not need a window: with --null-render the engine makes no
     * window and no GPU device, and ticks with a fixed delta. See LaunchOptions
     * for the launch flags.
     */
    class Engine
    {
    public:
        Engine(const Engine&) = delete;
        Engine& operator=(const Engine&) = delete;

        FLING_API Engine() = default;

        /** Stops play first if Shutdown did not run (an exception out of Tick), so no thread outlives the engine. */
        FLING_API ~Engine();

        /**
         * Run the engine (Startup, Tick until should stop, and shutdown)
         *
         * @return The process exit code, one of the Fling::ExitCode values
         */
        template <class T_GameType>
        int Run(int argc, const char* argv[]);

        /**
         * Run the engine with developer UI (for example an editor that derives from
         * Fling::BaseEditor) drawn every frame. Under --null-render there is no
         * developer UI.
         *
         * @return The process exit code, one of the Fling::ExitCode values
         */
        template <class T_GameType, class T_DebugUIType>
        int Run(int argc, const char* argv[]);

    private:
        /**
         * Start any systems or subsystems that may be needed
         *
         * @return ExitCode::BadArguments if a launch flag is not valid, otherwise ExitCode::Success
         */
        ExitCode Startup(int argc, const char* argv[]);

        /** Startup: read the command line and the launch flags, start the log, and seed the random numbers. */
        ExitCode ReadLaunchOptions(int argc, const char* argv[]);

        /** Startup: load EngineConf.ini and resolve the run settings (m_Settings). */
        ExitCode ResolveSettings();

        /** Startup: the resource manager, the timing, the headless clock and the input. */
        void StartSystems();

        /** Startup: start --replay-input and --record-input. */
        ExitCode StartInputFiles();

        /** Startup: the world, the renderer (or none headless), the frame cap's default, the modules and physics. */
        void MakeWorld();

        /** Startup: the world's clock, rate, exec mode and input sources, the run controls and the frame limiter. */
        ExitCode WireWorld();

        /** Startup: the developer UI and the window's input. */
        void StartDebugUI();

        /**
         * Load the string tables, pick the language, and make them the ones that
         * DisplayText uses. Runs --loc-import and --loc-export.
         *
         * @return ExitCode::Failure if a localization tool fails, otherwise ExitCode::Success
         */
        ExitCode StartLocalization();

        /**
         * Run the main loop, one iteration per frame, until the window closes, the world
         * quits, or --frames frames have run. With --dump-schedule or --validate-schedule, start the world,
         * report the system schedule, and return without running the loop.
         *
         * @return ExitCode::Failure if the system schedule has errors, otherwise ExitCode::Success
         */
        ExitCode RunFrames();

        /** True while the main loop must run one more frame. */
        bool ShouldTick() const;

        /** The frame's input poll (this thread writes the input in every exec mode), then the headless clock step. */
        void PumpInput(float t_DeltaTime);

        /** Carry out the AppRequests that the ticks sent (the cursor). */
        void HandleAppRequests();

        /**
         * Render the frame (windowed) with the VSync setting, note the focus and the monitor,
         * then wait for the frame limiter.
         */
        void Present(float t_DeltaTime);

        /** The refresh rate of the window's monitor now. Empty headless. */
        std::optional<uint32> WindowMonitorHz() const;

        /**
         * Give the frame limiter the rate it keeps now (RunControls::LimiterFps: the cap, the
         * present mode in use and the background cap in one rule).
         */
        void UpdateFrameLimiter();

        /** The tick that --frames stops at: one tick every --frames-per-tick frames. 0: no limit. */
        uint64 LastTickOfRun() const;

        /**
         * Shutdown all engine systems and do any necessary cleanup
         *
         * @param t_Code The exit code of this run, written in the exit summary
         * @return t_Code
         */
        ExitCode Shutdown(ExitCode t_Code);

        /** Write a one-line summary of this run to stdout, in the --log-format. */
        void PrintExitSummary(ExitCode t_Code) const;

        /**
         * Persistent world object that can be used to load levels, entities, etc. The engine
         * owns it. The game and the debug UI keep a weak_ptr to it.
         */
        std::shared_ptr<World> m_World;

        /**
         * The implementation of the game that this engine is running. @see Fling::Game
         * The engine and the world share it, so it lives until both let go.
         */
        std::shared_ptr<Fling::Game> m_GameImpl;

        /** Optional developer UI (for example the editor). Drawn in the ImGui subpass. */
        std::shared_ptr<Fling::IDebugUI> m_DebugUI;

        /** Launch flags read from the command line in Startup */
        LaunchOptions m_Options;

        /** The run settings as Startup resolved them. They do not change after Startup. */
        RunSettings m_Settings;

        /** The live run settings, which the GPU Options window and the window system change. */
        std::unique_ptr<RunControls> m_Controls;

        /** Caps the main thread's frames at RunControls::LimiterFps. It does not touch the game thread. */
        FrameLimiter m_FrameLimiter;

        /** How often Present reads the refresh rate of the window's monitor again. */
        static constexpr int64 MonitorCheckIntervalNs = 1'000'000'000;

        /** The platform time of the next monitor check. */
        int64 m_NextMonitorCheckNs = 0;

        /** The seed of the random number generator. Pass it to --seed to repeat this run. */
        uint32 m_Seed = 0;

        /** Frames that the main loop has started. */
        uint64 m_FramesRun = 0;

        /** The world's tick count when it shut down, for the exit summary. */
        uint64 m_TicksAtExit = 0;

        /** True after Startup started the engine systems, which Shutdown must then stop */
        bool m_SystemsStarted = false;

        /** The ID of the GLFW input interface. Invalid under --null-render. */
        InputInterfaceId m_GlfwInputId;

        /** The window that the GLFW input interface was told about. */
        WindowHandle m_InputWindow;

        /** Real time when Startup began, for the exit summary */
        std::chrono::steady_clock::time_point m_StartTime;

        /**
         * Under --null-render: the platform clock. Frame f's input poll is at
         * HeadlessFrameNs(f) and its ticks are due at HeadlessFrameNs(f + 1), so a headless
         * run has the same input times and tick windows on every machine, in every exec mode.
         */
        std::unique_ptr<PlatformTime::VirtualClock> m_HeadlessClock;

        /** --dump-ticks: the open file. */
        std::unique_ptr<std::ofstream> m_DumpTicksFile;

        /** The length of a tick at the run's simulation rate (SimRate::StepNs). */
        int64 TickNs() const;

        int64 HeadlessFrameNs(uint64 t_Frame) const;

        /** Writes the ledger rows of the ticks since the last call to the --dump-ticks file (main thread). */
        void DumpNewTicks();

        /** The exec mode's policies: the world's mode and last tick, and for threaded play the game thread and input.
         */
        void SetUpExecMode();
    };

    template <class T_GameType, class T_DebugUIType>
    int Engine::Run(int argc, const char* argv[])
    {
        static_assert(std::is_default_constructible<T_DebugUIType>::value,
                      "T_DebugUIType requires default-constructible elements");
        static_assert(std::is_base_of<Fling::IDebugUI, T_DebugUIType>::value,
                      "T_DebugUIType must inherit from Fling::IDebugUI");
        m_DebugUI = std::make_shared<T_DebugUIType>();

        return Run<T_GameType>(argc, argv);
    }

    template <class T_GameType>
    int Engine::Run(int argc, const char* argv[])
    {
        static_assert(std::is_default_constructible<T_GameType>::value,
                      "T_GameType requires default-constructible elements");
        static_assert(std::is_base_of<Fling::Game, T_GameType>::value, "T_GameType must inherit from Fling::Game");

        m_GameImpl = std::make_shared<T_GameType>();

        ExitCode Code = Startup(argc, argv);
        // A localization tool (--loc-export, --loc-import) does its work in Startup
        if (Code == ExitCode::Success && !m_Options.IsLocalizationTool())
        {
            Code = RunFrames();
        }

        return ToInt(Shutdown(Code));
    }
} // namespace Fling
