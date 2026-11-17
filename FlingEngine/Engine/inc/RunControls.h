#pragma once

#include "FlingTypes.h"
#include "RunSettings.h"
#include "Strings/String.h"

#include <optional>

namespace Fling
{
    class World;

    /**
     * A count per second, measured over windows of WindowNs on a clock that the caller
     * passes in: the rate is the growth of the total over the last whole window.
     */
    class RateMeter
    {
    public:
        static constexpr int64 WindowNs = 1'000'000'000;

        /** The total so far (for example the world's tick count) at the clock time t_NowNs. */
        void Add(int64 t_NowNs, uint64 t_Total);

        /** The count per second over the last whole window. 0 before the first one ends. */
        double PerSecond() const { return m_PerSecond; }

    private:
        bool m_Started = false;
        int64 m_WindowStartNs = 0;
        uint64 m_WindowStartTotal = 0;
        double m_PerSecond = 0.0;
    };

    /**
     * The live values of the run settings that change while the game runs: the ones a person
     * sets in the editor's GPU Options window (VSync, the frame cap, the simulation rate,
     * render interpolation and frustum culling), and the ones the window system reports (the monitor's refresh
     * rate, and whether the window is in the background). They start from the RunSettings
     * that the engine resolved at startup, which do not change. Each setter checks its
     * request, so the window only draws and calls them.
     *
     * - VSync: Engine::Present gives it to VulkanApp::SetVSync; the swap chain is made again
     *   on the resize path, after the next present.
     * - MaxFps: Engine::Present puts LimiterFps into the FrameLimiter, on the next frame.
     * - Interpolation: World::SetInterpolation, which holds it, on the next frame.
     * - FrustumCulling: Engine::Present gives it to VulkanApp::SetFrustumCulling, on the next frame.
     * - OcclusionCulling: Engine::Present gives it to VulkanApp::SetOcclusionCulling, on the next frame.
     * - SimHz: World::RequestSimHz, a SetRate clock command applied between two ticks, at the
     *   next wake. The world holds the rate.
     *
     * No setting changes a replay or a headless run: VSync, the cap and interpolation change
     * only when frames are shown, and the simulation rate goes through the tick-stamped clock
     * command path, never through shared state, and is refused while an input replay plays or
     * a recording runs (a recording, format 2, stores one tick rate for its cuts). In live
     * windowed play the input is polled once a frame, so the frame rate (the cap, VSync)
     * decides how late a message reaches the queue, and so which tick it goes to.
     *
     * Main thread only.
     */
    class RunControls
    {
    public:
        /** t_Startup are the engine's resolved settings. t_World must outlive this. */
        RunControls(const RunSettings& t_Startup, World& t_World);

        /** On: present waits for the display (FIFO). Off: the lowest latency mode the surface offers. */
        bool VSync() const { return m_VSync; }
        void SetVSync(bool t_On);

        /** The frame cap in frames a second. 0: no cap. */
        uint32 MaxFps() const { return m_MaxFps; }

        /**
         * The frame cap: 0 (no cap) or 1 to RunSettings::MaxMaxFps. False, with a message, for
         * any other value. From then on the cap no longer follows the monitor.
         */
        bool SetMaxFps(uint32 t_Fps, String& t_OutError);

        /** Render interpolation, as the world has it (World::GetInterpolation). */
        bool Interpolation() const;
        void SetInterpolation(bool t_On);

        /** Frustum culling in the renderer (RunSettings::FrustumCulling). */
        bool FrustumCulling() const { return m_FrustumCulling; }
        void SetFrustumCulling(bool t_On);

        /** Occlusion culling in the renderer (RunSettings::OcclusionCulling). */
        bool OcclusionCulling() const { return m_OcclusionCulling; }
        void SetOcclusionCulling(bool t_On);

        /**
         * Ask for a new simulation rate (World::RequestSimHz, applied at the next tick
         * boundary). False, with the reason, for a rate that is not from 1 to 1000, or while
         * an input replay plays or a recording runs. The same rate as now is accepted and
         * changes nothing.
         */
        bool RequestSimHz(uint32 t_Hz, String& t_OutError);

        /** The rate the ticks run at now (World::GetSimHz). */
        uint32 SimHz() const;

        /**
         * The refresh rate of the monitor that the window is on now (empty or 0: not known).
         * When the frame cap came from the monitor and no one has set it, it follows.
         */
        void SetMonitorHz(std::optional<uint32> t_Hz);
        std::optional<uint32> MonitorHz() const { return m_MonitorHz; }

        /** True while the window does not have the focus: the cap is at most RunSettings::BackgroundMaxFps. */
        void SetInBackground(bool t_InBackground) { m_InBackground = t_InBackground; }
        bool InBackground() const { return m_InBackground; }

        /** The rate the frame limiter keeps now (Fling::LimiterFps). 0: it does not wait. */
        uint32 LimiterFps(bool t_DisplayPacesFrames) const;

        /**
         * What the frame limiter does now, in words, for the GPU Options window: "The limiter
         * keeps 144 fps", "The display paces the frames", "No cap", or "In the background: at
         * most 30 fps".
         */
        String DescribeLimiter(bool t_DisplayPacesFrames) const;

        /**
         * The measured rates, labelled, for the GPU Options window. In the background it says
         * that the background cap applies, so a low number is not read as a slow game.
         */
        String DescribeMeasured(double t_FramesPerSecond) const;

        /**
         * Once per frame, on the frame clock: count the ticks per second, and give the input
         * recorder and playback the rate the ticks run at, after a change.
         */
        void OnFrame(int64 t_NowNs);

        /** The ticks per second that ran, over the last RateMeter window. */
        double TicksPerSecond() const { return m_Ticks.PerSecond(); }

    private:
        World& m_World;
        RateMeter m_Ticks;

        /** The rate the input recorder and playback have. */
        uint32 m_InputTickRate = 0;

        bool m_VSync = true;
        bool m_FrustumCulling = true;
        bool m_OcclusionCulling = true;
        uint32 m_MaxFps = 0;

        /** True while the cap is the monitor's refresh rate and no one has set it. */
        bool m_MaxFpsFollowsMonitor = false;

        std::optional<uint32> m_MonitorHz;
        bool m_InBackground = false;
    };
} // namespace Fling
