#include <catch2/catch_all.hpp>

#include "LaunchOptions.h"
#include "Misc/CommandLine.h"
#include <string>
#include <vector>

namespace
{
    /** Parse launch options from the given args. The first arg is the executable name. */
    bool ParseArgs(std::vector<const char*> t_Args, Fling::LaunchOptions& t_Out, std::string& t_OutError)
    {
        t_Args.insert(t_Args.begin(), "FlingEngine.exe");
        REQUIRE(Fling::CommandLine::Get().Init(static_cast<int>(t_Args.size()), t_Args.data()));
        return Fling::LaunchOptions::Parse(Fling::CommandLine::Get(), t_Out, t_OutError);
    }
} // namespace

TEST_CASE("Launch Options", "[Launch Options]")
{
    using namespace Fling;

    LaunchOptions Options;
    std::string Error;

    SECTION("Defaults with no flags")
    {
        REQUIRE(ParseArgs({}, Options, Error));

        REQUIRE_FALSE(Options.NullRender);
        REQUIRE_FALSE(Options.Unattended);
        REQUIRE(Options.Frames == 0);
        REQUIRE_FALSE(Options.HasSeed);
        REQUIRE(Options.Format == LogFormat::Text);
        REQUIRE(Error.empty());
    }

    SECTION("All headless flags")
    {
        REQUIRE(ParseArgs({"--null-render", "--unattended", "--frames", "300", "--seed", "42", "--log-format", "json"},
                          Options,
                          Error));

        REQUIRE(Options.NullRender);
        REQUIRE(Options.Unattended);
        REQUIRE(Options.Frames == 300);
        REQUIRE(Options.HasSeed);
        REQUIRE(Options.Seed == 42);
        REQUIRE(Options.Format == LogFormat::Json);
    }

    SECTION("Equals syntax")
    {
        REQUIRE(ParseArgs({"--frames=5", "--seed=0", "--null-render=false"}, Options, Error));

        REQUIRE(Options.Frames == 5);
        REQUIRE(Options.HasSeed);
        REQUIRE(Options.Seed == 0);
        REQUIRE_FALSE(Options.NullRender);
    }

    SECTION("Largest seed")
    {
        REQUIRE(ParseArgs({"--seed", "4294967295"}, Options, Error));
        REQUIRE(Options.Seed == 4294967295u);
    }

    SECTION("Frames that is not a number")
    {
        REQUIRE_FALSE(ParseArgs({"--frames", "abc"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--frames"));
    }

    SECTION("Frames with no value")
    {
        // "--frames" with no value has the value "true", which is not a number
        REQUIRE_FALSE(ParseArgs({"--frames"}, Options, Error));
    }

    SECTION("Negative frames")
    {
        REQUIRE_FALSE(ParseArgs({"--frames=-1"}, Options, Error));
    }

    SECTION("Frames with text after the number")
    {
        REQUIRE_FALSE(ParseArgs({"--frames", "10x"}, Options, Error));
    }

    SECTION("Seed that is too large")
    {
        REQUIRE_FALSE(ParseArgs({"--seed", "4294967296"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--seed"));
    }

    SECTION("Replay and record input paths")
    {
        REQUIRE(ParseArgs({"--replay-input", "a.input.jsonl", "--record-input=b.input.jsonl"}, Options, Error));
        REQUIRE(Options.ReplayInput == "a.input.jsonl");
        REQUIRE(Options.RecordInput == "b.input.jsonl");
    }

    SECTION("Input paths default to empty")
    {
        REQUIRE(ParseArgs({}, Options, Error));
        REQUIRE(Options.ReplayInput.empty());
        REQUIRE(Options.RecordInput.empty());
    }

    SECTION("Replay input with no value")
    {
        REQUIRE_FALSE(ParseArgs({"--replay-input"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--replay-input needs a file path"));
    }

    SECTION("Record input with an empty value")
    {
        REQUIRE_FALSE(ParseArgs({"--record-input="}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--record-input needs a file path"));
    }

    SECTION("Unknown log format")
    {
        REQUIRE_FALSE(ParseArgs({"--log-format", "xml"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--log-format"));
    }

    SECTION("Bool flag with a bad value")
    {
        REQUIRE_FALSE(ParseArgs({"--null-render=maybe"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--null-render"));
    }

    SECTION("Input logging")
    {
        REQUIRE(ParseArgs({}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::Off);

        REQUIRE(ParseArgs({"--log-input"}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::Events);

        REQUIRE(ParseArgs({"--log-input=all"}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::All);

        REQUIRE(ParseArgs({"--log-input=off"}, Options, Error));
        REQUIRE(Options.InputLog == InputLogLevel::Off);
    }

    SECTION("Unknown input log level")
    {
        REQUIRE_FALSE(ParseArgs({"--log-input=loud"}, Options, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--log-input"));
    }
}
