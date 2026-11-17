#include "LaunchOptions.h"
#include "RunSettings.h"
#include "Strings/StringUtils.h"

namespace Fling
{
    namespace
    {
        /** How a message names a setting: the flag for the command line, the key for the config file. */
        String SourceLabel(const SettingSource t_Source, const StringView t_Flag, const StringView t_Key)
        {
            return t_Source == SettingSource::CommandLine ? "--" + String(t_Flag)
                                                          : "[Engine] " + String(t_Key) + " in EngineConf.ini";
        }

        /** The whole number range of one setting, and how a message names its unit. */
        struct NumberRange
        {
            uint32 Min = 0;
            uint32 Max = 0;

            /** 0 is valid too (no cap). */
            bool AllowZero = false;

            const char* Unit = "";
        };

        /** Check t_Value against t_Range. Returns false with a message that names t_Label. */
        bool CheckRange(const uint32 t_Value, const NumberRange& t_Range, const StringView t_Label, String& t_OutError)
        {
            if ((t_Range.AllowZero && t_Value == 0) || (t_Value >= t_Range.Min && t_Value <= t_Range.Max))
            {
                return true;
            }
            t_OutError = String(t_Label) + " must be " + (t_Range.AllowZero ? String("0 (no cap) or ") : String()) +
                         "from " + std::to_string(t_Range.Min) + " to " + std::to_string(t_Range.Max) + " " +
                         t_Range.Unit + ", not " + std::to_string(t_Value);
            return false;
        }

        /**
         * One whole number setting: the config key is checked if it has text, then the flag
         * wins over it. Leaves t_Out and t_OutSource as they are when neither is given.
         */
        bool PickNumber(const std::optional<uint32>& t_Flag,
                        const StringView t_ConfigText,
                        const StringView t_FlagName,
                        const StringView t_Key,
                        const NumberRange& t_Range,
                        uint32& t_Out,
                        SettingSource& t_OutSource,
                        String& t_OutError)
        {
            if (!t_ConfigText.empty())
            {
                const String Label = SourceLabel(SettingSource::Config, t_FlagName, t_Key);
                uint32 Value = 0;
                if (!StringUtils::TryParseUint32(t_ConfigText, Value))
                {
                    t_OutError = Label + " must be a whole number, not '" + String(t_ConfigText) + "'";
                    return false;
                }
                if (!CheckRange(Value, t_Range, Label, t_OutError))
                {
                    return false;
                }
                t_Out = Value;
                t_OutSource = SettingSource::Config;
            }
            if (t_Flag)
            {
                if (!CheckRange(
                        *t_Flag, t_Range, SourceLabel(SettingSource::CommandLine, t_FlagName, t_Key), t_OutError))
                {
                    return false;
                }
                t_Out = *t_Flag;
                t_OutSource = SettingSource::CommandLine;
            }
            return true;
        }

        /** As PickNumber, for an on|off setting. */
        bool PickOnOff(const std::optional<bool>& t_Flag,
                       const StringView t_ConfigText,
                       const StringView t_Key,
                       bool& t_Out,
                       SettingSource& t_OutSource,
                       String& t_OutError)
        {
            if (!t_ConfigText.empty())
            {
                if (!ParseOnOff(t_ConfigText, t_Out))
                {
                    t_OutError = "[Engine] " + String(t_Key) + " in EngineConf.ini must be on or off, not '" +
                                 String(t_ConfigText) + "'";
                    return false;
                }
                t_OutSource = SettingSource::Config;
            }
            if (t_Flag)
            {
                t_Out = *t_Flag;
                t_OutSource = SettingSource::CommandLine;
            }
            return true;
        }

        /** As PickNumber, for the exec mode. */
        bool PickExec(const std::optional<ExecMode>& t_Flag,
                      const StringView t_ConfigText,
                      ExecMode& t_Out,
                      SettingSource& t_OutSource,
                      String& t_OutError)
        {
            if (!t_ConfigText.empty())
            {
                if (!ParseExecMode(t_ConfigText, t_Out))
                {
                    t_OutError = "[Engine] " + String(RunSettingsConfig::ExecKey) +
                                 " in EngineConf.ini must be direct, inline, or threaded, not '" +
                                 String(t_ConfigText) + "'";
                    return false;
                }
                t_OutSource = SettingSource::Config;
            }
            if (t_Flag)
            {
                t_Out = *t_Flag;
                t_OutSource = SettingSource::CommandLine;
            }
            return true;
        }

        const char* OnOffName(const bool t_On)
        {
            return t_On ? "on" : "off";
        }
    } // namespace

    const char* SettingSourceName(const SettingSource t_Source)
    {
        switch (t_Source)
        {
        case SettingSource::Config:
            return "config";
        case SettingSource::CommandLine:
            return "command line";
        case SettingSource::Monitor:
            return "monitor";
        case SettingSource::Default:
        default:
            return "default";
        }
    }

    bool RunSettings::Resolve(const LaunchOptions& t_Options,
                              const RunSettingsConfig& t_Config,
                              RunSettings& t_Out,
                              String& t_OutError)
    {
        using Keys = RunSettingsConfig;
        t_Out = RunSettings{};
        t_OutError.clear();
        t_Out.Headless = t_Options.IsHeadless();
        t_Out.HeadlessFlag = t_Options.Offscreen ? "--offscreen" : "--null-render";

        const NumberRange SimHzRange{.Min = SimRate::MinHz, .Max = SimRate::MaxHz, .Unit = "ticks a second"};
        const NumberRange MaxFpsRange{.Min = 1, .Max = MaxMaxFps, .AllowZero = true, .Unit = "frames a second"};
        if (!PickExec(t_Options.Exec, t_Config.Exec, t_Out.Exec, t_Out.ExecSource, t_OutError) ||
            !PickNumber(t_Options.SimHz,
                        t_Config.SimHz,
                        "sim-hz",
                        Keys::SimHzKey,
                        SimHzRange,
                        t_Out.SimHz,
                        t_Out.SimHzSource,
                        t_OutError) ||
            !PickNumber(t_Options.MaxFps,
                        t_Config.MaxFps,
                        "max-fps",
                        Keys::MaxFpsKey,
                        MaxFpsRange,
                        t_Out.MaxFps,
                        t_Out.MaxFpsSource,
                        t_OutError) ||
            !PickOnOff(t_Options.VSync, t_Config.VSync, Keys::VSyncKey, t_Out.VSync, t_Out.VSyncSource, t_OutError) ||
            !PickOnOff(t_Options.Interpolation,
                       t_Config.Interpolation,
                       Keys::InterpolationKey,
                       t_Out.Interpolation,
                       t_Out.InterpolationSource,
                       t_OutError))
        {
            return false;
        }

        // A flag that only one exec mode uses is an error in the others, not silently ignored
        if (t_Options.JitterMaxMicroseconds > 0 && t_Out.Exec != ExecMode::Threaded)
        {
            t_OutError = String("--thread-jitter is for --exec threaded, not ") + ExecModeName(t_Out.Exec);
            return false;
        }
        if (t_Out.Headless && t_Options.VSync)
        {
            t_Out.Ignored.push_back(String("--vsync does nothing with ") + t_Out.HeadlessFlag +
                                    ": there is no swap chain");
        }
        return true;
    }

    void RunSettings::ResolveFrameCap(const std::optional<uint32> t_MonitorHz)
    {
        MonitorHz.reset();
        if (!Headless && t_MonitorHz && *t_MonitorHz > 0)
        {
            MonitorHz = t_MonitorHz;
        }
        if (MaxFpsSource != SettingSource::Default && MaxFpsSource != SettingSource::Monitor)
        {
            return;
        }
        if (Headless)
        {
            MaxFps = 0;
            MaxFpsSource = SettingSource::Default;
        }
        else if (MonitorHz)
        {
            MaxFps = MonitorMaxFps(*MonitorHz);
            MaxFpsSource = SettingSource::Monitor;
        }
        else
        {
            MaxFps = FallbackMaxFps;
            MaxFpsSource = SettingSource::Default;
        }
    }

    String RunSettings::Describe() const
    {
        String Cap = MaxFps == 0 ? String("none") : std::to_string(MaxFps) + " fps";
        String CapSource = SettingSourceName(MaxFpsSource);
        if (MaxFpsSource == SettingSource::Default)
        {
            CapSource =
                Headless ? String("default for ") + HeadlessFlag : "default: the monitor refresh rate is not known";
        }
        else if (MaxFpsSource == SettingSource::Monitor)
        {
            CapSource = "the monitor's refresh rate";
        }
        return "tick rate " + std::to_string(SimHz) + " Hz (" + SettingSourceName(SimHzSource) + "), exec " +
               ExecModeName(Exec) + " (" + SettingSourceName(ExecSource) + "), frame cap " + Cap + " (" + CapSource +
               "), vsync " + OnOffName(VSync) + " (" + SettingSourceName(VSyncSource) + "), interpolation " +
               OnOffName(Interpolation) + " (" + SettingSourceName(InterpolationSource) + ")";
    }

    uint32 LimiterFps(const uint32 t_MaxFps,
                      const std::optional<uint32> t_MonitorHz,
                      const bool t_DisplayPacesFrames,
                      const bool t_InBackground)
    {
        uint32 Cap = t_MaxFps;
        if (t_InBackground && (Cap == 0 || Cap > RunSettings::BackgroundMaxFps))
        {
            Cap = RunSettings::BackgroundMaxFps;
        }
        if (t_DisplayPacesFrames && t_MonitorHz && Cap >= *t_MonitorHz)
        {
            return 0;
        }
        return Cap;
    }
} // namespace Fling
