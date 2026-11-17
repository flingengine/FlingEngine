#include "Input/Input.h"
#include "Logger.h"
#include "RunControls.h"
#include "World.h"

#include <cstdio>
#include <string>

namespace Fling
{
    void RateMeter::Add(const int64 t_NowNs, const uint64 t_Total)
    {
        if (!m_Started)
        {
            m_Started = true;
            m_WindowStartNs = t_NowNs;
            m_WindowStartTotal = t_Total;
            return;
        }
        const int64 ElapsedNs = t_NowNs - m_WindowStartNs;
        if (ElapsedNs < WindowNs)
        {
            return;
        }
        m_PerSecond = static_cast<double>(t_Total - m_WindowStartTotal) * 1e9 / static_cast<double>(ElapsedNs);
        m_WindowStartNs = t_NowNs;
        m_WindowStartTotal = t_Total;
    }

    RunControls::RunControls(const RunSettings& t_Startup, World& t_World)
        : m_World(t_World),
          m_InputTickRate(t_Startup.SimHz),
          m_VSync(t_Startup.VSync),
          m_FrustumCulling(t_Startup.FrustumCulling),
          m_OcclusionCulling(t_Startup.OcclusionCulling),
          m_MaxFps(t_Startup.MaxFps),
          m_MaxFpsFollowsMonitor(t_Startup.MaxFpsSource == SettingSource::Monitor),
          m_MonitorHz(t_Startup.MonitorHz)
    {
    }

    void RunControls::SetVSync(const bool t_On)
    {
        if (t_On != m_VSync)
        {
            m_VSync = t_On;
            F_LOG_TRACE("VSync {}", t_On ? "on" : "off");
        }
    }

    void RunControls::SetFrustumCulling(const bool t_On)
    {
        if (t_On != m_FrustumCulling)
        {
            m_FrustumCulling = t_On;
            F_LOG_TRACE("Frustum culling {}", t_On ? "on" : "off");
        }
    }

    void RunControls::SetOcclusionCulling(const bool t_On)
    {
        if (t_On != m_OcclusionCulling)
        {
            m_OcclusionCulling = t_On;
            F_LOG_TRACE("Occlusion culling {}", t_On ? "on" : "off");
        }
    }

    bool RunControls::SetMaxFps(const uint32 t_Fps, String& t_OutError)
    {
        t_OutError.clear();
        if (t_Fps > RunSettings::MaxMaxFps)
        {
            t_OutError = "The frame cap must be 0 (no cap) or from 1 to " + std::to_string(RunSettings::MaxMaxFps) +
                         " frames a second, not " + std::to_string(t_Fps);
            return false;
        }
        m_MaxFps = t_Fps;
        m_MaxFpsFollowsMonitor = false;
        F_LOG_TRACE("Frame cap {} fps (0: none)", t_Fps);
        return true;
    }

    bool RunControls::Interpolation() const
    {
        return m_World.GetInterpolation();
    }

    void RunControls::SetInterpolation(const bool t_On)
    {
        m_World.SetInterpolation(t_On);
        F_LOG_TRACE("Render interpolation {}", t_On ? "on" : "off");
    }

    bool RunControls::RequestSimHz(const uint32 t_Hz, String& t_OutError)
    {
        return m_World.RequestSimHz(t_Hz, t_OutError);
    }

    uint32 RunControls::SimHz() const
    {
        return m_World.GetSimHz();
    }

    void RunControls::SetMonitorHz(std::optional<uint32> t_Hz)
    {
        if (t_Hz == 0u)
        {
            t_Hz.reset();
        }
        if (t_Hz == m_MonitorHz)
        {
            return;
        }
        m_MonitorHz = t_Hz;
        if (m_MaxFpsFollowsMonitor && m_MonitorHz)
        {
            m_MaxFps = RunSettings::MonitorMaxFps(*m_MonitorHz);
            F_LOG_TRACE("Monitor refresh rate {} Hz: the frame cap follows it", *m_MonitorHz);
        }
    }

    uint32 RunControls::LimiterFps(const bool t_DisplayPacesFrames) const
    {
        return Fling::LimiterFps(m_MaxFps, m_MonitorHz, t_DisplayPacesFrames, m_InBackground);
    }

    String RunControls::DescribeLimiter(const bool t_DisplayPacesFrames) const
    {
        const uint32 Fps = LimiterFps(t_DisplayPacesFrames);
        if (Fps == 0)
        {
            return t_DisplayPacesFrames ? "The display paces the frames" : "No cap";
        }
        if (m_InBackground && Fps == RunSettings::BackgroundMaxFps && Fps != m_MaxFps)
        {
            return "In the background: at most " + std::to_string(Fps) + " fps";
        }
        return "The limiter keeps " + std::to_string(Fps) + " fps";
    }

    String RunControls::DescribeMeasured(const double t_FramesPerSecond) const
    {
        char Text[160];
        std::snprintf(Text,
                      sizeof(Text),
                      "Measured: %.0f frames a second, %.1f ticks a second",
                      t_FramesPerSecond,
                      TicksPerSecond());
        String Measured = Text;
        if (m_InBackground)
        {
            Measured += " (window in the background: at most " + std::to_string(RunSettings::BackgroundMaxFps) +
                        " frames a second)";
        }
        return Measured;
    }

    void RunControls::OnFrame(const int64 t_NowNs)
    {
        m_Ticks.Add(t_NowNs, m_World.GetTickCount());

        // A recording started after a rate change notes the new rate, and a replay is checked
        // against it. An active recorder or playback refuses a new rate (SetTickRate)
        const uint32 Hz = SimHz();
        if (Hz != 0 && Hz != m_InputTickRate)
        {
#if WITH_INPUT_RECORDING
            InputRecorder& Recorder = Input::Get().GetRecorder();
            InputPlayback& Playback = Input::Get().GetPlayback();
            if (Recorder.IsActive() || Playback.IsActive())
            {
                return;
            }
            Recorder.SetTickRate(Hz);
            Playback.SetTickRate(Hz);
#endif
            m_InputTickRate = Hz;
        }
    }
} // namespace Fling
