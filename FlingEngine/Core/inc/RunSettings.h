#pragma once

#include "CoreAPI.h"
#include "ExecMode.h"
#include "FlingTypes.h"
#include "SimRate.h"
#include "Strings/String.h"

#include <optional>
#include <vector>

namespace Fling
{
    struct LaunchOptions;

    /** Where a run setting came from, for the startup log line. */
    enum class SettingSource : uint8
    {
        /** The engine's default. */
        Default,

        /** The [Engine] section of Config/EngineConf.ini. */
        Config,

        /** A launch flag. */
        CommandLine,

        /** The refresh rate of the monitor that the window is on (the frame cap only). */
        Monitor,
    };

    /** "default", "config", "command line" or "monitor". */
    CORE_API const char* SettingSourceName(SettingSource t_Source);

    /**
     * The text of the [Engine] keys that RunSettings reads. Empty text (the key is not in the
     * file, or has no value) means that the default applies.
     */
    struct RunSettingsConfig
    {
        /** The key names in EngineConf.ini, also used in the error messages. */
        static constexpr const char* ExecKey = "Exec";
        static constexpr const char* SimHzKey = "SimHz";
        static constexpr const char* MaxFpsKey = "MaxFps";
        static constexpr const char* VSyncKey = "VSync";
        static constexpr const char* InterpolationKey = "Interpolation";
        static constexpr const char* FrustumCullingKey = "FrustumCulling";
        static constexpr const char* OcclusionCullingKey = "OcclusionCulling";

        String Exec;
        String SimHz;
        String MaxFps;
        String VSync;
        String Interpolation;
        String FrustumCulling;
        String OcclusionCulling;

        /**
         * Each key's text from t_GetKey, which returns the text of one [Engine] key, or empty
         * text when the key is missing (FlingConfig::GetString("Engine", Key, "")).
         */
        template <class T_GetKey>
        static RunSettingsConfig Read(T_GetKey&& t_GetKey)
        {
            return RunSettingsConfig{.Exec = t_GetKey(ExecKey),
                                     .SimHz = t_GetKey(SimHzKey),
                                     .MaxFps = t_GetKey(MaxFpsKey),
                                     .VSync = t_GetKey(VSyncKey),
                                     .Interpolation = t_GetKey(InterpolationKey),
                                     .FrustumCulling = t_GetKey(FrustumCullingKey),
                                     .OcclusionCulling = t_GetKey(OcclusionCullingKey)};
        }
    };

    /**
     * The settings that decide how a run ticks and shows its frames, resolved once at startup,
     * each with one precedence: the command line, then [Engine] in the config file, then the
     * default. Every key that is in the file is checked, also when a flag wins over it, so a
     * typo in EngineConf.ini is an error in every run.
     *
     * | Setting | Key | Flag | Default |
     * | --- | --- | --- | --- |
     * | Exec | [Engine] Exec | --exec | threaded (DefaultExecMode) |
     * | SimHz | [Engine] SimHz | --sim-hz | 30 (SimRate::DefaultHz), 1 to 1000 |
     * | MaxFps | [Engine] MaxFps | --max-fps | monitor refresh, else 60; none headless. 0: no cap |
     * | VSync | [Engine] VSync | --vsync | on |
     * | Interpolation | [Engine] Interpolation | --interpolation | on |
     * | FrustumCulling | [Engine] FrustumCulling | --frustum-culling | on |
     * | OcclusionCulling | [Engine] OcclusionCulling | --occlusion-culling | on |
     *
     * The values here do not change after startup. The GPU Options window changes VSync, the
     * frame cap, the simulation rate, interpolation, frustum culling and occlusion culling while the game runs;
     * RunControls holds those live values, starting from these. Edit mode in the editor always runs on one thread,
     * whatever Exec says.
     */
    struct RunSettings
    {
        /** The frame cap when no setting gives one and the monitor's refresh rate is not known. */
        static constexpr uint32 FallbackMaxFps = 60;

        /** The highest frame cap that a setting may ask for. */
        static constexpr uint32 MaxMaxFps = 1000;

        /** The frame cap while the window is in the background, so a hidden game does not take the whole GPU. */
        static constexpr uint32 BackgroundMaxFps = 30;

        ExecMode Exec = DefaultExecMode;
        SettingSource ExecSource = SettingSource::Default;

        uint32 SimHz = SimRate::DefaultHz;
        SettingSource SimHzSource = SettingSource::Default;

        /** Frames a second at most on the main thread. 0: no cap. */
        uint32 MaxFps = 0;
        SettingSource MaxFpsSource = SettingSource::Default;

        /**
         * On: present waits for the display's refresh (FIFO). Off: frames do not wait and may
         * tear, for the lowest latency (ChoosePresentMode in Graphics has the mapping).
         */
        bool VSync = true;
        SettingSource VSyncSource = SettingSource::Default;

        /** The renderer draws between the latest two published ticks; off: the latest tick only. */
        bool Interpolation = true;
        SettingSource InterpolationSource = SettingSource::Default;

        /**
         * The renderer does not draw a mesh whose bounds are outside the camera's view this
         * frame (MeshCuller in Graphics). Presentation only: the simulation never reads it.
         */
        bool FrustumCulling = true;
        SettingSource FrustumCullingSource = SettingSource::Default;

        /**
         * The renderer does not draw a mesh that other meshes hide from the camera (the GPU
         * occlusion test in Graphics). Presentation only: the simulation never reads it.
         */
        bool OcclusionCulling = true;
        SettingSource OcclusionCullingSource = SettingSource::Default;

        /** The refresh rate of the window's monitor at startup (ResolveFrameCap). Empty when not known, and headless.
         */
        std::optional<uint32> MonitorHz;

        /** True for a --null-render run: no monitor, and no frame cap unless a setting asks for one. */
        bool Headless = false;

        /** The flag that made the run headless, for messages: "--null-render" or "--offscreen". */
        const char* HeadlessFlag = "--null-render";

        /** Settings that were given but do nothing in this run, one line each, for the startup log. */
        std::vector<String> Ignored;

        /**
         * Read the settings from the launch flags and the [Engine] keys, and check them.
         *
         * @param t_Options     The launch flags (LaunchOptions::Parse)
         * @param t_Config      The text of the [Engine] keys
         * @param t_Out         Receives the settings. MaxFps keeps its default source until
         *                      ResolveFrameCap when neither source sets it
         * @param t_OutError    Receives a message that names the key or the flag of the first
         *                      value that is not valid
         * @return False if a value is not valid, or a flag is for another exec mode
         */
        CORE_API static bool Resolve(const LaunchOptions& t_Options,
                                     const RunSettingsConfig& t_Config,
                                     RunSettings& t_Out,
                                     String& t_OutError);

        /** The frame cap that a monitor's refresh rate gives: t_MonitorHz, at most MaxMaxFps. */
        static constexpr uint32 MonitorMaxFps(const uint32 t_MonitorHz)
        {
            return t_MonitorHz < MaxMaxFps ? t_MonitorHz : MaxMaxFps;
        }

        /**
         * The default frame cap, when neither the command line nor the config file set one:
         * none for a headless run (so headless runs and tests stay as fast as before), else
         * MonitorMaxFps(t_MonitorHz), else FallbackMaxFps when the refresh rate is missing or 0.
         *
         * @param t_MonitorHz   The refresh rate of the window's monitor. Empty when it is not known
         */
        CORE_API void ResolveFrameCap(std::optional<uint32> t_MonitorHz);

        /**
         * One line for the startup log with each value and where it came from, for example
         * "tick rate 30 Hz (default), exec threaded (default), frame cap 144 fps (the monitor's refresh rate),
         * vsync on (default), interpolation on (default), frustum culling on (default)".
         */
        CORE_API String Describe() const;
    };

    /**
     * The rate the frame limiter keeps (0: it does not wait), the one place that says how the
     * frame cap, the present mode in use and the background cap combine: the lower of the cap
     * and the display's refresh rate wins.
     *
     * - The display does not pace the frames (VSync off): the cap.
     * - The display paces the frames (FIFO): present already waits for the display, so a cap
     *   at or above the monitor's refresh rate would only fight it (a limiter at 59 fps
     *   against a 59.94 Hz display misses one refresh a second): the limiter then does not
     *   wait. A lower cap still applies.
     * - In the background the cap is at most RunSettings::BackgroundMaxFps.
     *
     * @param t_MaxFps              The frame cap. 0: none
     * @param t_MonitorHz           The refresh rate of the window's monitor. Empty when it is not known
     * @param t_DisplayPacesFrames  True when the present mode in use waits for the display
     * @param t_InBackground        True while the window does not have the focus
     */
    CORE_API uint32 LimiterFps(uint32 t_MaxFps,
                               std::optional<uint32> t_MonitorHz,
                               bool t_DisplayPacesFrames,
                               bool t_InBackground);
} // namespace Fling
