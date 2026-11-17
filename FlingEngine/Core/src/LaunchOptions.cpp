#include "LaunchOptions.h"
#include "Misc/CommandLine.h"
#include "Strings/String.h"
#include "Strings/StringUtils.h"

#include <algorithm>

namespace Fling
{
    namespace
    {
        constexpr StringView NullRenderFlag = "null-render";
        constexpr StringView OffscreenFlag = "offscreen";
        constexpr StringView ResolutionFlag = "resolution";
        constexpr StringView ScreenshotFlag = "screenshot";
        constexpr StringView ScreenshotTicksFlag = "screenshot-ticks";
        constexpr StringView DumpStateFlag = "dump-state";
        constexpr StringView DumpStateTicksFlag = "dump-state-ticks";
        constexpr StringView UnattendedFlag = "unattended";
        constexpr StringView ProfileFlag = "profile";
        constexpr StringView FramesFlag = "frames";
        constexpr StringView FramesPerTickFlag = "frames-per-tick";
        constexpr StringView SeedFlag = "seed";
        constexpr StringView LogFormatFlag = "log-format";
        constexpr StringView LevelFlag = "level";
        constexpr StringView LogInputFlag = "log-input";
        constexpr StringView ReplayInputFlag = "replay-input";
        constexpr StringView RecordInputFlag = "record-input";
        constexpr StringView DumpScheduleFlag = "dump-schedule";
        constexpr StringView ValidateScheduleFlag = "validate-schedule";
        constexpr StringView LanguageFlag = "language";
        constexpr StringView LocExportFlag = "loc-export";
        constexpr StringView LocImportFlag = "loc-import";
        constexpr StringView LocGatherFlag = "loc-gather";
        constexpr StringView DumpTicksFlag = "dump-ticks";
        constexpr StringView ExecFlag = "exec";
        constexpr StringView SimHzFlag = "sim-hz";
        constexpr StringView MaxFpsFlag = "max-fps";
        constexpr StringView VSyncFlag = "vsync";
        constexpr StringView InterpolationFlag = "interpolation";
        constexpr StringView FrustumCullingFlag = "frustum-culling";
        constexpr StringView OcclusionCullingFlag = "occlusion-culling";
        constexpr StringView GpuBlendFlag = "gpu-blend";
        constexpr StringView ThreadJitterFlag = "thread-jitter";
        /**
         * Flags that are refused with the name of the flag that does their job, so a script
         * that passes one does not run unseen.
         */
        struct RefusedFlag
        {
            StringView Flag;
            const char* Instead;
        };
        constexpr RefusedFlag RefusedFlags[] = {
            {"render-hz", "--max-fps caps the frame rate, --sim-hz sets the tick rate"},
            {"present-mode", "--vsync on|off chooses how frames are shown"},
        };

        using StringUtils::TryParseUint32;

        /** Read a flag that must have a file path. A flag with no value or an empty value is an error. */
        bool ReadPath(const CommandLine& t_CmdLine, StringView t_Flag, String& t_Out, String& t_OutError)
        {
            const std::optional<StringView> Value = t_CmdLine.GetValueAsString(t_Flag);
            if (!Value)
            {
                return true;
            }

            const StringView Str = *Value;
            // A flag with no value has the value "true"
            if (Str.empty() || Str == "true")
            {
                t_OutError = "--" + String(t_Flag) + " needs a file path";
                return false;
            }

            t_Out = String(Str);
            return true;
        }

        /** Read a bool flag. A flag with no value is "true". */
        bool ReadBool(const CommandLine& t_CmdLine, StringView t_Flag, bool& t_Out, String& t_OutError)
        {
            const std::optional<StringView> Value = t_CmdLine.GetValueAsString(t_Flag);
            if (!Value)
            {
                return true;
            }

            const StringView Str = *Value;
            if (Str == "true" || Str == "1")
            {
                t_Out = true;
                return true;
            }
            if (Str == "false" || Str == "0")
            {
                t_Out = false;
                return true;
            }

            t_OutError = "--" + String(t_Flag) + " must be true or false, not '" + String(Str) + "'";
            return false;
        }

        /** Read a flag that must be a whole number from 0 to the uint32 maximum. */
        bool
        ReadUint32(const CommandLine& t_CmdLine, StringView t_Flag, uint32& t_Out, bool& t_OutFound, String& t_OutError)
        {
            t_OutFound = false;

            const std::optional<StringView> Value = t_CmdLine.GetValueAsString(t_Flag);
            if (!Value)
            {
                return true;
            }

            const StringView Str = *Value;
            if (!TryParseUint32(Str, t_Out))
            {
                t_OutError =
                    "--" + String(t_Flag) + " needs a whole number from 0 to 4294967295, not '" + String(Str) + "'";
                return false;
            }

            t_OutFound = true;
            return true;
        }

        /** Read an on|off flag into an optional: empty when the flag is not given. */
        bool ReadOptionalOnOff(const CommandLine& t_CmdLine,
                               StringView t_Flag,
                               std::optional<bool>& t_Out,
                               String& t_OutError)
        {
            const std::optional<StringView> Value = t_CmdLine.GetValueAsString(t_Flag);
            if (!Value)
            {
                return true;
            }
            bool bOn = false;
            if (!ParseOnOff(*Value, bOn))
            {
                t_OutError = "--" + String(t_Flag) + " must be on or off, not '" + String(*Value) + "'";
                return false;
            }
            t_Out = bOn;
            return true;
        }

        /** Read a list of tick numbers, such as 30,60,120 (each 1 or more). The result is sorted, with no repeats. */
        bool
        ReadTickList(const CommandLine& t_CmdLine, StringView t_Flag, std::vector<uint64>& t_Out, String& t_OutError)
        {
            const std::optional<StringView> Value = t_CmdLine.GetValueAsString(t_Flag);
            if (!Value)
            {
                return true;
            }
            StringView Rest = *Value;
            const auto Fail = [&]()
            {
                t_OutError = "--" + String(t_Flag) +
                             " needs a comma list of ticks (1 or more), for example 30,60,120, not '" + String(*Value) +
                             "'";
                return false;
            };
            if (Rest.empty() || Rest == "true")
            {
                return Fail();
            }
            while (true)
            {
                const size_t Comma = Rest.find(',');
                const StringView Item = Rest.substr(0, Comma);
                uint32 Tick = 0;
                if (!TryParseUint32(Item, Tick) || Tick == 0)
                {
                    return Fail();
                }
                t_Out.push_back(Tick);
                if (Comma == StringView::npos)
                {
                    break;
                }
                Rest = Rest.substr(Comma + 1);
            }
            std::sort(t_Out.begin(), t_Out.end());
            t_Out.erase(std::unique(t_Out.begin(), t_Out.end()), t_Out.end());
            return true;
        }

        /** Read a uint32 flag into an optional: empty when the flag is not given. */
        bool ReadOptionalUint32(const CommandLine& t_CmdLine,
                                StringView t_Flag,
                                std::optional<uint32>& t_Out,
                                String& t_OutError)
        {
            uint32 Value = 0;
            bool bFound = false;
            if (!ReadUint32(t_CmdLine, t_Flag, Value, bFound, t_OutError))
            {
                return false;
            }
            if (bFound)
            {
                t_Out = Value;
            }
            return true;
        }
    } // namespace

    bool LaunchOptions::Parse(const CommandLine& t_CmdLine, LaunchOptions& t_Out, String& t_OutError)
    {
        t_Out = LaunchOptions{};
        t_OutError.clear();

        if (!ReadBool(t_CmdLine, NullRenderFlag, t_Out.NullRender, t_OutError))
        {
            return false;
        }

        if (!ReadBool(t_CmdLine, UnattendedFlag, t_Out.Unattended, t_OutError) ||
            !ReadBool(t_CmdLine, ProfileFlag, t_Out.Profile, t_OutError))
        {
            return false;
        }
#if !WITH_PROFILER
        if (t_Out.Profile)
        {
            t_OutError = "--profile is not available in this build: configure with -DFLING_PROFILER=ON";
            return false;
        }
#endif

        if (!ReadBool(t_CmdLine, OffscreenFlag, t_Out.Offscreen, t_OutError))
        {
            return false;
        }
        if (t_Out.Offscreen && t_Out.NullRender)
        {
            t_OutError = "--offscreen and --null-render cannot be used together: --offscreen renders, --null-render "
                         "does not";
            return false;
        }

        if (const std::optional<StringView> Resolution = t_CmdLine.GetValueAsString(ResolutionFlag))
        {
            const StringView Str = *Resolution;
            const size_t X = Str.find('x');
            uint32 Width = 0;
            uint32 Height = 0;
            if (X == StringView::npos || !TryParseUint32(Str.substr(0, X), Width) ||
                !TryParseUint32(Str.substr(X + 1), Height) || Width == 0 || Height == 0 || Width > 8192 ||
                Height > 8192)
            {
                t_OutError = "--resolution needs <width>x<height> from 1 to 8192, for example 1280x720, not '" +
                             String(Str) + "'";
                return false;
            }
            t_Out.ResolutionWidth = Width;
            t_Out.ResolutionHeight = Height;
        }

        if (!ReadPath(t_CmdLine, ScreenshotFlag, t_Out.Screenshot, t_OutError) ||
            !ReadTickList(t_CmdLine, ScreenshotTicksFlag, t_Out.ScreenshotTicks, t_OutError) ||
            !ReadPath(t_CmdLine, DumpStateFlag, t_Out.DumpState, t_OutError) ||
            !ReadTickList(t_CmdLine, DumpStateTicksFlag, t_Out.DumpStateTicks, t_OutError))
        {
            return false;
        }
        if (!t_Out.ScreenshotTicks.empty() && t_Out.Screenshot.empty())
        {
            t_OutError = "--screenshot-ticks needs --screenshot <file.png>";
            return false;
        }
        if (!t_Out.DumpStateTicks.empty() && t_Out.DumpState.empty())
        {
            t_OutError = "--dump-state-ticks needs --dump-state <file.json>";
            return false;
        }
        if (!t_Out.Screenshot.empty() && t_Out.NullRender)
        {
            t_OutError = "--screenshot needs a renderer: use --offscreen instead of --null-render (or --dump-state "
                         "for the state as text)";
            return false;
        }

        if (!ReadBool(t_CmdLine, DumpScheduleFlag, t_Out.DumpSchedule, t_OutError) ||
            !ReadBool(t_CmdLine, ValidateScheduleFlag, t_Out.ValidateSchedule, t_OutError))
        {
            return false;
        }

        bool bFoundFrames = false;
        if (!ReadUint32(t_CmdLine, FramesFlag, t_Out.Frames, bFoundFrames, t_OutError))
        {
            return false;
        }

        bool bFoundFramesPerTick = false;
        if (!ReadUint32(t_CmdLine, FramesPerTickFlag, t_Out.FramesPerTick, bFoundFramesPerTick, t_OutError))
        {
            return false;
        }
        if (t_Out.FramesPerTick == 0)
        {
            t_OutError = "--frames-per-tick must be 1 or more";
            return false;
        }

        if (!ReadUint32(t_CmdLine, SeedFlag, t_Out.Seed, t_Out.HasSeed, t_OutError))
        {
            return false;
        }

        if (const std::optional<StringView> Format = t_CmdLine.GetValueAsString(LogFormatFlag))
        {
            const StringView Str = *Format;
            if (Str == "json")
            {
                t_Out.Format = LogFormat::Json;
            }
            else if (Str == "text")
            {
                t_Out.Format = LogFormat::Text;
            }
            else
            {
                t_OutError = "--log-format must be text or json, not '" + String(Str) + "'";
                return false;
            }
        }

        if (const std::optional<StringView> Level = t_CmdLine.GetValueAsString(LogInputFlag))
        {
            const StringView Str = *Level;
            if (Str == "true" || Str == "1" || Str == "events")
            {
                t_Out.InputLog = InputLogLevel::Events;
            }
            else if (Str == "all")
            {
                t_Out.InputLog = InputLogLevel::All;
            }
            else if (Str == "false" || Str == "0" || Str == "off")
            {
                t_Out.InputLog = InputLogLevel::Off;
            }
            else
            {
                t_OutError = "--log-input must be events, all, or off, not '" + String(Str) + "'";
                return false;
            }
        }

        if (!ReadPath(t_CmdLine, LevelFlag, t_Out.Level, t_OutError))
        {
            return false;
        }

        if (const std::optional<StringView> Language = t_CmdLine.GetValueAsString(LanguageFlag))
        {
            const StringView Str = *Language;
            // A flag with no value has the value "true"
            if (Str.empty() || Str == "true")
            {
                t_OutError = "--language needs a language code, for example --language fr";
                return false;
            }
            t_Out.Language = String(Str);
        }

        if (!ReadPath(t_CmdLine, LocExportFlag, t_Out.LocExport, t_OutError) ||
            !ReadPath(t_CmdLine, LocImportFlag, t_Out.LocImport, t_OutError) ||
            !ReadPath(t_CmdLine, LocGatherFlag, t_Out.LocGather, t_OutError))
        {
            return false;
        }

        // The localization tools only read and write files, so they need no window
        if (t_Out.IsLocalizationTool())
        {
            t_Out.NullRender = true;
        }

        if (!ReadPath(t_CmdLine, DumpTicksFlag, t_Out.DumpTicks, t_OutError))
        {
            return false;
        }

        if (const std::optional<StringView> Exec = t_CmdLine.GetValueAsString(ExecFlag))
        {
            ExecMode Mode = DefaultExecMode;
            if (!ParseExecMode(*Exec, Mode))
            {
                t_OutError = "--exec must be direct, inline, or threaded, not '" + String(*Exec) + "'";
                return false;
            }
            t_Out.Exec = Mode;
        }

        if (!ReadOptionalUint32(t_CmdLine, SimHzFlag, t_Out.SimHz, t_OutError) ||
            !ReadOptionalUint32(t_CmdLine, MaxFpsFlag, t_Out.MaxFps, t_OutError) ||
            !ReadOptionalOnOff(t_CmdLine, VSyncFlag, t_Out.VSync, t_OutError) ||
            !ReadOptionalOnOff(t_CmdLine, InterpolationFlag, t_Out.Interpolation, t_OutError) ||
            !ReadOptionalOnOff(t_CmdLine, FrustumCullingFlag, t_Out.FrustumCulling, t_OutError) ||
            !ReadOptionalOnOff(t_CmdLine, OcclusionCullingFlag, t_Out.OcclusionCulling, t_OutError) ||
            !ReadOptionalOnOff(t_CmdLine, GpuBlendFlag, t_Out.GpuBlend, t_OutError))
        {
            return false;
        }
        for (const RefusedFlag& Refused : RefusedFlags)
        {
            if (t_CmdLine.GetValueAsString(Refused.Flag).has_value())
            {
                t_OutError = "--" + String(Refused.Flag) + " is not a flag: " + Refused.Instead;
                return false;
            }
        }

        if (const std::optional<StringView> Jitter = t_CmdLine.GetValueAsString(ThreadJitterFlag))
        {
            const StringView Str = *Jitter;
            const size_t Colon = Str.find(':');
            if (Colon == StringView::npos || !TryParseUint32(Str.substr(0, Colon), t_Out.JitterSeed) ||
                !TryParseUint32(Str.substr(Colon + 1), t_Out.JitterMaxMicroseconds))
            {
                t_OutError =
                    "--thread-jitter needs <seed>:<max microseconds>, for example 7:2000, not '" + String(Str) + "'";
                return false;
            }
        }

#if WITH_INPUT_RECORDING
        if (!ReadPath(t_CmdLine, ReplayInputFlag, t_Out.ReplayInput, t_OutError))
        {
            return false;
        }

        if (!ReadPath(t_CmdLine, RecordInputFlag, t_Out.RecordInput, t_OutError))
        {
            return false;
        }
#else
        // A shipped game cannot be driven by an input file
        for (const StringView Flag : {ReplayInputFlag, RecordInputFlag})
        {
            if (t_CmdLine.GetValueAsString(Flag).has_value())
            {
                t_OutError = "--" + String(Flag) + " is not available in shipping builds";
                return false;
            }
        }
#endif

        // A capture with no tick list is at the last tick of the run, so the run must have one
        if (t_Out.Frames == 0)
        {
            if (!t_Out.Screenshot.empty() && t_Out.ScreenshotTicks.empty())
            {
                t_OutError = "--screenshot needs --frames N (a frame at the last tick) or --screenshot-ticks";
                return false;
            }
            if (!t_Out.DumpState.empty() && t_Out.DumpStateTicks.empty())
            {
                t_OutError = "--dump-state needs --frames N (the state at the last tick) or --dump-state-ticks";
                return false;
            }
        }

        return true;
    }

    String CaptureFilePath(const StringView t_Path, const uint64 t_Tick, const bool t_ManyTicks)
    {
        String Path(t_Path);
        const String TickText = std::to_string(t_Tick);
        constexpr StringView Placeholder = "{tick}";
        if (const size_t At = Path.find(Placeholder); At != String::npos)
        {
            Path.replace(At, Placeholder.size(), TickText);
            return Path;
        }
        if (!t_ManyTicks)
        {
            return Path;
        }
        // Before the extension, if the file name has one
        const size_t Slash = Path.find_last_of("/\\");
        const size_t Dot = Path.find_last_of('.');
        const bool bHasExtension = Dot != String::npos && (Slash == String::npos || Dot > Slash);
        const size_t InsertAt = bHasExtension ? Dot : Path.size();
        Path.insert(InsertAt, "_tick" + TickText);
        return Path;
    }
} // namespace Fling
