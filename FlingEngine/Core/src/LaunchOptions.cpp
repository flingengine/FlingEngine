#include "LaunchOptions.h"
#include "Misc/CommandLine.h"

#include <charconv>
#include <limits>
#include <string_view>

namespace Fling
{
    namespace
    {
        constexpr std::string_view NullRenderFlag = "null-render";
        constexpr std::string_view UnattendedFlag = "unattended";
        constexpr std::string_view FramesFlag = "frames";
        constexpr std::string_view SeedFlag = "seed";
        constexpr std::string_view LogFormatFlag = "log-format";
        constexpr std::string_view LevelFlag = "level";
        constexpr std::string_view LogInputFlag = "log-input";
        constexpr std::string_view ReplayInputFlag = "replay-input";
        constexpr std::string_view RecordInputFlag = "record-input";
        constexpr std::string_view DumpScheduleFlag = "dump-schedule";
        constexpr std::string_view ValidateScheduleFlag = "validate-schedule";
        constexpr std::string_view LanguageFlag = "language";
        constexpr std::string_view LocExportFlag = "loc-export";
        constexpr std::string_view LocImportFlag = "loc-import";
        constexpr std::string_view LocGatherFlag = "loc-gather";

        /** Read a flag that must have a file path. A flag with no value or an empty value is an error. */
        bool ReadPath(const CommandLine& t_CmdLine, std::string_view t_Flag, std::string& t_Out, std::string& t_OutError)
        {
            const char* Value = t_CmdLine.GetValueAsString(t_Flag);
            if (Value == nullptr)
            {
                return true;
            }

            const std::string_view Str = Value;
            // A flag with no value has the value "true"
            if (Str.empty() || Str == "true")
            {
                t_OutError = "--" + std::string(t_Flag) + " needs a file path";
                return false;
            }

            t_Out = std::string(Str);
            return true;
        }

        /** Read a bool flag. A flag with no value is "true". */
        bool ReadBool(const CommandLine& t_CmdLine, std::string_view t_Flag, bool& t_Out, std::string& t_OutError)
        {
            const char* Value = t_CmdLine.GetValueAsString(t_Flag);
            if (Value == nullptr)
            {
                return true;
            }

            const std::string_view Str = Value;
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

            t_OutError = "--" + std::string(t_Flag) + " must be true or false, not '" + std::string(Str) + "'";
            return false;
        }

        /** Read a flag that must be a whole number from 0 to the uint32 maximum. */
        bool ReadUint32(const CommandLine& t_CmdLine,
                        std::string_view t_Flag,
                        uint32& t_Out,
                        bool& t_OutFound,
                        std::string& t_OutError)
        {
            t_OutFound = false;

            const char* Value = t_CmdLine.GetValueAsString(t_Flag);
            if (Value == nullptr)
            {
                return true;
            }

            const std::string_view Str = Value;
            uint64 Result = 0;
            const auto [End, Error] = std::from_chars(Str.data(), Str.data() + Str.size(), Result);
            if (Str.empty() || Error != std::errc() || End != Str.data() + Str.size() ||
                Result > std::numeric_limits<uint32>::max())
            {
                t_OutError = "--" + std::string(t_Flag) + " needs a whole number from 0 to 4294967295, not '" +
                             std::string(Str) + "'";
                return false;
            }

            t_Out = static_cast<uint32>(Result);
            t_OutFound = true;
            return true;
        }
    } // namespace

    bool LaunchOptions::Parse(const CommandLine& t_CmdLine, LaunchOptions& t_Out, std::string& t_OutError)
    {
        t_Out = LaunchOptions{};
        t_OutError.clear();

        if (!ReadBool(t_CmdLine, NullRenderFlag, t_Out.NullRender, t_OutError))
        {
            return false;
        }

        if (!ReadBool(t_CmdLine, UnattendedFlag, t_Out.Unattended, t_OutError))
        {
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

        if (!ReadUint32(t_CmdLine, SeedFlag, t_Out.Seed, t_Out.HasSeed, t_OutError))
        {
            return false;
        }

        if (const char* Format = t_CmdLine.GetValueAsString(LogFormatFlag))
        {
            const std::string_view Str = Format;
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
                t_OutError = "--log-format must be text or json, not '" + std::string(Str) + "'";
                return false;
            }
        }

        if (const char* Level = t_CmdLine.GetValueAsString(LogInputFlag))
        {
            const std::string_view Str = Level;
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
                t_OutError = "--log-input must be events, all, or off, not '" + std::string(Str) + "'";
                return false;
            }
        }

        if (!ReadPath(t_CmdLine, LevelFlag, t_Out.Level, t_OutError))
        {
            return false;
        }

        if (const char* Language = t_CmdLine.GetValueAsString(LanguageFlag))
        {
            const std::string_view Str = Language;
            // A flag with no value has the value "true"
            if (Str.empty() || Str == "true")
            {
                t_OutError = "--language needs a language code, for example --language fr";
                return false;
            }
            t_Out.Language = std::string(Str);
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
        for (const std::string_view Flag : {ReplayInputFlag, RecordInputFlag})
        {
            if (t_CmdLine.GetValueAsString(Flag) != nullptr)
            {
                t_OutError = "--" + std::string(Flag) + " is not available in shipping builds";
                return false;
            }
        }
#endif

        return true;
    }
} // namespace Fling
