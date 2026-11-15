#include "../Common/LaunchArgs.h"
#include "LaunchOptions.h"
#include "Misc/CommandLine.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <vector>

using Fling::String;
using Fling::StringView;

using TestArgs::ParseLaunchArgs;

TEST_CASE("Launch Options", "[Launch Options]")
{
    using namespace Fling;

    LaunchOptions Options;
    String Error;

    SECTION("Defaults with no flags")
    {
        REQUIRE(ParseLaunchArgs({}, Options, Error));

        REQUIRE_FALSE(Options.NullRender);
        REQUIRE_FALSE(Options.Unattended);
        REQUIRE(Options.Frames == 0);
        REQUIRE_FALSE(Options.HasSeed);
        REQUIRE(Options.Format == LogFormat::Text);
        REQUIRE(Error.empty());
    }

    SECTION("All headless flags")
    {
        REQUIRE(ParseLaunchArgs(
            {"--null-render", "--unattended", "--frames", "300", "--seed", "42", "--log-format", "json"},
            Options,
            Error));

        REQUIRE(Options.NullRender);
        REQUIRE(Options.Unattended);
        REQUIRE(Options.Frames == 300);
        REQUIRE(Options.HasSeed);
        REQUIRE(Options.Seed == 42);
        REQUIRE(Options.Format == LogFormat::Json);
    }

    SECTION("Schedule flags")
    {
        REQUIRE(ParseLaunchArgs({}, Options, Error));
        REQUIRE_FALSE(Options.DumpSchedule);
        REQUIRE_FALSE(Options.ValidateSchedule);

        REQUIRE(ParseLaunchArgs({"--dump-schedule", "--validate-schedule"}, Options, Error));
        REQUIRE(Options.DumpSchedule);
        REQUIRE(Options.ValidateSchedule);
    }

    SECTION("Level override")
    {
        REQUIRE(ParseLaunchArgs({}, Options, Error));
        REQUIRE(Options.Level.empty());

        REQUIRE(ParseLaunchArgs({"--level", "Levels/PhysicsTest.json"}, Options, Error));
        REQUIRE(Options.Level == "Levels/PhysicsTest.json");
    }

    SECTION("Level with no path is an error")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--level"}, Options, Error));
        REQUIRE(Error.find("--level needs a file path") != String::npos);
    }

    SECTION("Language")
    {
        REQUIRE(ParseLaunchArgs({}, Options, Error));
        REQUIRE(Options.Language.empty());

        REQUIRE(ParseLaunchArgs({"--language", "fr"}, Options, Error));
        REQUIRE(Options.Language == "fr");
        REQUIRE_FALSE(Options.IsLocalizationTool());

        REQUIRE_FALSE(ParseLaunchArgs({"--language"}, Options, Error));
        REQUIRE(Error.find("--language needs a language code") != String::npos);
    }

    SECTION("Localization tools run with no window")
    {
        REQUIRE(ParseLaunchArgs({"--loc-export", "Strings.csv"}, Options, Error));
        REQUIRE(Options.LocExport == "Strings.csv");
        REQUIRE(Options.IsLocalizationTool());
        REQUIRE(Options.NullRender);

        LaunchOptions ImportOptions;
        REQUIRE(ParseLaunchArgs({"--loc-import", "Strings.csv"}, ImportOptions, Error));
        REQUIRE(ImportOptions.LocImport == "Strings.csv");
        REQUIRE(ImportOptions.NullRender);

        LaunchOptions GatherOptions;
        REQUIRE(ParseLaunchArgs({"--loc-gather", "Sandbox"}, GatherOptions, Error));
        REQUIRE(GatherOptions.LocGather == "Sandbox");
        REQUIRE(GatherOptions.IsLocalizationTool());
        REQUIRE(GatherOptions.NullRender);

        REQUIRE_FALSE(ParseLaunchArgs({"--loc-export"}, Options, Error));
        REQUIRE(Error.find("--loc-export needs a file path") != String::npos);
    }

    SECTION("Equals syntax")
    {
        REQUIRE(ParseLaunchArgs({"--frames=5", "--seed=0", "--null-render=false"}, Options, Error));

        REQUIRE(Options.Frames == 5);
        REQUIRE(Options.HasSeed);
        REQUIRE(Options.Seed == 0);
        REQUIRE_FALSE(Options.NullRender);
    }

    SECTION("Largest seed")
    {
        REQUIRE(ParseLaunchArgs({"--seed", "4294967295"}, Options, Error));
        REQUIRE(Options.Seed == 4294967295u);
    }

    SECTION("Frames that is not a number")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--frames", "abc"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--frames"));
    }

    SECTION("Frames with no value")
    {
        // "--frames" with no value has the value "true", which is not a number
        REQUIRE_FALSE(ParseLaunchArgs({"--frames"}, Options, Error));
    }

    SECTION("Negative frames")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--frames=-1"}, Options, Error));
    }

    SECTION("Frames with text after the number")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--frames", "10x"}, Options, Error));
    }

    SECTION("Seed that is too large")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--seed", "4294967296"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--seed"));
    }

#if WITH_INPUT_RECORDING
    SECTION("Replay and record input paths")
    {
        REQUIRE(ParseLaunchArgs({"--replay-input", "a.input.jsonl", "--record-input=b.input.jsonl"}, Options, Error));
        REQUIRE(Options.ReplayInput == "a.input.jsonl");
        REQUIRE(Options.RecordInput == "b.input.jsonl");
    }

    SECTION("Input paths default to empty")
    {
        REQUIRE(ParseLaunchArgs({}, Options, Error));
        REQUIRE(Options.ReplayInput.empty());
        REQUIRE(Options.RecordInput.empty());
    }

    SECTION("Replay input with no value")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--replay-input"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--replay-input needs a file path"));
    }

    SECTION("Record input with an empty value")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--record-input="}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--record-input needs a file path"));
    }
#else
    SECTION("Replay and record input are errors in shipping builds")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--replay-input", "a.input.jsonl"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--replay-input is not available in shipping builds"));
        REQUIRE_FALSE(ParseLaunchArgs({"--record-input", "b.input.jsonl"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--record-input is not available in shipping builds"));
    }
#endif

    SECTION("Unknown log format")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--log-format", "xml"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--log-format"));
    }

    SECTION("Bool flag with a bad value")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--null-render=maybe"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--null-render"));
    }

    SECTION("Input logging")
    {
        REQUIRE(ParseLaunchArgs({}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::Off);

        REQUIRE(ParseLaunchArgs({"--log-input"}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::Events);

        REQUIRE(ParseLaunchArgs({"--log-input=all"}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::All);

        REQUIRE(ParseLaunchArgs({"--log-input=off"}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::Off);
    }

    SECTION("Unknown input log level")
    {
        REQUIRE_FALSE(ParseLaunchArgs({"--log-input=loud"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--log-input"));
    }

    SECTION("Execution mode, simulation rate, frame cap and thread jitter")
    {
        // Not given: empty, so RunSettings takes the config file or the default
        REQUIRE(ParseLaunchArgs({}, Options, Error));
        REQUIRE_FALSE(Options.Exec.has_value());
        REQUIRE_FALSE(Options.SimHz.has_value());
        REQUIRE_FALSE(Options.MaxFps.has_value());
        REQUIRE(Options.JitterMaxMicroseconds == 0);

        REQUIRE(ParseLaunchArgs({"--exec", "inline"}, Options, Error));
        REQUIRE(Options.Exec == ExecMode::Inline);

        REQUIRE(ParseLaunchArgs(
            {"--exec", "threaded", "--sim-hz", "60", "--max-fps", "144", "--thread-jitter", "7:2000"}, Options, Error));
        REQUIRE(Options.Exec == ExecMode::Threaded);
        REQUIRE(Options.SimHz == 60u);
        REQUIRE(Options.MaxFps == 144u);
        REQUIRE(Options.JitterSeed == 7);
        REQUIRE(Options.JitterMaxMicroseconds == 2000);

        // The range is RunSettings' rule; here only a whole number is required
        REQUIRE(ParseLaunchArgs({"--max-fps", "0", "--sim-hz", "0"}, Options, Error));
        REQUIRE(Options.MaxFps == 0u);
        REQUIRE(Options.SimHz == 0u);
        REQUIRE_FALSE(ParseLaunchArgs({"--sim-hz", "-30"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--sim-hz"));
        REQUIRE_FALSE(ParseLaunchArgs({"--max-fps", "fast"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--max-fps"));

        REQUIRE_FALSE(ParseLaunchArgs({"--exec", "parallel"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--exec"));

        REQUIRE_FALSE(ParseLaunchArgs({"--thread-jitter", "2000"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--thread-jitter"));

        // Not silently ignored: it names the flags that set the rates
        REQUIRE_FALSE(ParseLaunchArgs({"--render-hz", "60"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--max-fps"));

        // VSync is the one present setting
        REQUIRE_FALSE(ParseLaunchArgs({"--present-mode", "mailbox"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--present-mode is not a flag: --vsync on|off"));
    }
}
