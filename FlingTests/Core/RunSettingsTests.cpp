#include "../Common/LaunchArgs.h"
#include "LaunchOptions.h"
#include "RunSettings.h"
#include "SimRate.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <map>
#include <vector>

using Fling::String;

namespace
{
    using namespace Fling;

    /** Read the launch flags, then the run settings with t_Config. */
    bool
    Resolve(std::vector<const char*> t_Args, const RunSettingsConfig& t_Config, RunSettings& t_Out, String& t_OutError)
    {
        LaunchOptions Options;
        REQUIRE(TestArgs::ParseLaunchArgs(std::move(t_Args), Options, t_OutError));
        return RunSettings::Resolve(Options, t_Config, t_Out, t_OutError);
    }
} // namespace

TEST_CASE("Run settings: the defaults are threaded play at 30 Hz with VSync and interpolation on", "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    REQUIRE(Resolve({}, {}, Settings, Error));
    REQUIRE(Settings.Exec == ExecMode::Threaded);
    REQUIRE(Settings.ExecSource == SettingSource::Default);
    REQUIRE(Settings.SimHz == 30);
    REQUIRE(Settings.SimHzSource == SettingSource::Default);
    REQUIRE(Settings.VSync);
    REQUIRE(Settings.VSyncSource == SettingSource::Default);
    REQUIRE(Settings.Interpolation);
    REQUIRE(Settings.InterpolationSource == SettingSource::Default);
    REQUIRE(Settings.Ignored.empty());
    REQUIRE(SimRate::StepNs(Settings.SimHz) == 33'333'333);
    REQUIRE(SimRate::StepNs(60) == 16'666'667);
}

TEST_CASE("Run settings: the frame cap is the monitor's refresh rate, else 60", "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    REQUIRE(Resolve({}, {}, Settings, Error));

    SECTION("A 144 Hz monitor")
    {
        Settings.ResolveFrameCap(144u);
        REQUIRE(Settings.MaxFps == 144);
        REQUIRE(Settings.MaxFpsSource == SettingSource::Monitor);
    }
    SECTION("A monitor above the highest cap")
    {
        Settings.ResolveFrameCap(2000u);
        REQUIRE(Settings.MaxFps == RunSettings::MaxMaxFps);
    }
    SECTION("A monitor that reports 0")
    {
        Settings.ResolveFrameCap(0u);
        REQUIRE(Settings.MaxFps == 60);
        REQUIRE(Settings.MaxFpsSource == SettingSource::Default);
    }
    SECTION("No monitor")
    {
        Settings.ResolveFrameCap(std::nullopt);
        REQUIRE(Settings.MaxFps == 60);
        REQUIRE(Settings.MaxFpsSource == SettingSource::Default);
    }
    SECTION("Headless runs have no cap unless they ask for one, so tests stay fast")
    {
        REQUIRE(Resolve({"--null-render"}, {}, Settings, Error));
        Settings.ResolveFrameCap(144u);
        REQUIRE(Settings.MaxFps == 0);
        REQUIRE_THAT(Settings.Describe(),
                     Catch::Matchers::ContainsSubstring("frame cap none (default for --null-render)"));

        REQUIRE(Resolve({"--null-render", "--max-fps", "30"}, {}, Settings, Error));
        Settings.ResolveFrameCap(std::nullopt);
        REQUIRE(Settings.MaxFps == 30);
        REQUIRE(Settings.MaxFpsSource == SettingSource::CommandLine);
    }
    SECTION("A set cap wins over the monitor; 0 is no cap")
    {
        REQUIRE(Resolve({}, {.MaxFps = "90"}, Settings, Error));
        Settings.ResolveFrameCap(144u);
        REQUIRE(Settings.MaxFps == 90);
        REQUIRE(Settings.MaxFpsSource == SettingSource::Config);

        REQUIRE(Resolve({"--max-fps", "0"}, {.MaxFps = "90"}, Settings, Error));
        Settings.ResolveFrameCap(144u);
        REQUIRE(Settings.MaxFps == 0);
        REQUIRE(Settings.MaxFpsSource == SettingSource::CommandLine);
    }
}

TEST_CASE("Run settings: the command line wins over the config file, which wins over the default", "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    const RunSettingsConfig Config{
        .Exec = "direct", .SimHz = "60", .MaxFps = "120", .VSync = "off", .Interpolation = "off"};

    REQUIRE(Resolve({}, Config, Settings, Error));
    REQUIRE(Settings.Exec == ExecMode::Direct);
    REQUIRE(Settings.ExecSource == SettingSource::Config);
    REQUIRE(Settings.SimHz == 60);
    REQUIRE(Settings.SimHzSource == SettingSource::Config);
    REQUIRE(Settings.MaxFps == 120);
    REQUIRE(Settings.MaxFpsSource == SettingSource::Config);
    REQUIRE_FALSE(Settings.VSync);
    REQUIRE(Settings.VSyncSource == SettingSource::Config);
    REQUIRE_FALSE(Settings.Interpolation);
    REQUIRE(Settings.InterpolationSource == SettingSource::Config);

    REQUIRE(Resolve({"--exec", "inline", "--sim-hz", "20", "--max-fps", "75", "--vsync", "on", "--interpolation", "on"},
                    Config,
                    Settings,
                    Error));
    Settings.ResolveFrameCap(144u);
    REQUIRE(Settings.Describe() == "tick rate 20 Hz (command line), exec inline (command line), frame cap 75 fps "
                                   "(command line), vsync on (command line), interpolation on (command line)");

    // A key with no value is the default
    REQUIRE(Resolve({}, RunSettingsConfig{}, Settings, Error));
    Settings.ResolveFrameCap(144u);
    REQUIRE(Settings.Describe() == "tick rate 30 Hz (default), exec threaded (default), frame cap 144 fps (the "
                                   "monitor's refresh rate), vsync on (default), interpolation on (default)");
}

TEST_CASE("Run settings: every config key is read by its name", "[RunSettings]")
{
    const std::map<String, String> File{{"Exec", "inline"},
                                        {"SimHz", "60"},
                                        {"MaxFps", "90"},
                                        {"VSync", "off"},
                                        {"Interpolation", "off"},
                                        {"PresentMode", "mailbox"}};
    const RunSettingsConfig Config = RunSettingsConfig::Read(
        [&File](const char* t_Key)
        {
            const auto Found = File.find(t_Key);
            return Found != File.end() ? Found->second : String();
        });
    REQUIRE(Config.Exec == "inline");
    REQUIRE(Config.SimHz == "60");
    REQUIRE(Config.MaxFps == "90");
    REQUIRE(Config.VSync == "off");
    REQUIRE(Config.Interpolation == "off");
}

TEST_CASE("Run settings: a bad value in a config key is an error, also when a flag wins over it", "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    const auto Fails = [&](std::vector<const char*> t_Args, const RunSettingsConfig& t_Config, const char* t_Message)
    {
        REQUIRE_FALSE(Resolve(std::move(t_Args), t_Config, Settings, Error));
        REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring(t_Message));
    };
    for (const bool bWithFlags : {false, true})
    {
        CAPTURE(bWithFlags);
        const auto Args = [bWithFlags](std::vector<const char*> t_Flags)
        {
            return bWithFlags ? t_Flags : std::vector<const char*>{};
        };
        Fails(Args({"--exec", "direct"}),
              {.Exec = "parallel"},
              "[Engine] Exec in EngineConf.ini must be direct, inline, or threaded, not 'parallel'");
        Fails(Args({"--sim-hz", "60"}),
              {.SimHz = "fast"},
              "[Engine] SimHz in EngineConf.ini must be a whole number, not 'fast'");
        Fails(Args({"--sim-hz", "60"}),
              {.SimHz = "0"},
              "[Engine] SimHz in EngineConf.ini must be from 1 to 1000 ticks a second, not 0");
        Fails(Args({"--max-fps", "60"}),
              {.MaxFps = "5000"},
              "[Engine] MaxFps in EngineConf.ini must be 0 (no cap) or from 1 to 1000");
        Fails(Args({"--vsync", "off"}), {.VSync = "sometimes"}, "[Engine] VSync in EngineConf.ini must be on or off");
        Fails(Args({"--interpolation", "on"}),
              {.Interpolation = "1"},
              "[Engine] Interpolation in EngineConf.ini must be on or off");
    }
}

TEST_CASE("Run settings: values out of range on the command line are errors that name the flag", "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    REQUIRE_FALSE(Resolve({"--sim-hz", "0"}, {}, Settings, Error));
    REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--sim-hz must be from 1 to 1000 ticks a second, not 0"));
    REQUIRE_FALSE(Resolve({"--sim-hz", "100000"}, {}, Settings, Error));
    REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--sim-hz"));
    REQUIRE(Resolve({"--sim-hz", "1000"}, {}, Settings, Error));
    REQUIRE(Resolve({"--sim-hz", "1"}, {}, Settings, Error));
    REQUIRE_FALSE(Resolve({"--max-fps", "5000"}, {}, Settings, Error));
    REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("--max-fps must be 0"));
    REQUIRE_FALSE(Resolve({}, {.SimHz = "-30"}, Settings, Error));
    REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("[Engine] SimHz in EngineConf.ini must be a whole number"));
}

TEST_CASE("Run settings: --thread-jitter is for --exec threaded only, from any source of the exec mode",
          "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    REQUIRE(Resolve({"--thread-jitter", "7:2000"}, {}, Settings, Error));
    REQUIRE(Resolve({"--thread-jitter", "7:2000", "--exec", "threaded"}, {.Exec = "direct"}, Settings, Error));
    REQUIRE_FALSE(Resolve({"--thread-jitter", "7:2000", "--exec", "inline"}, {}, Settings, Error));
    REQUIRE(Error == "--thread-jitter is for --exec threaded, not inline");
    REQUIRE_FALSE(Resolve({"--thread-jitter", "7:2000"}, {.Exec = "direct"}, Settings, Error));
    REQUIRE(Error == "--thread-jitter is for --exec threaded, not direct");
    // 0 microseconds is off, so it is no error anywhere
    REQUIRE(Resolve({"--thread-jitter", "7:0", "--exec", "direct"}, {}, Settings, Error));
}

TEST_CASE("Run settings: --vsync with --null-render does nothing, and says so", "[RunSettings]")
{
    RunSettings Settings;
    String Error;
    REQUIRE(Resolve({"--null-render", "--vsync", "off"}, {}, Settings, Error));
    REQUIRE(Settings.Ignored == std::vector<String>{"--vsync does nothing with --null-render: there is no swap chain"});
    // A key in the config file is the project's setting for every run, so it is not reported
    REQUIRE(Resolve({"--null-render"}, {.VSync = "off"}, Settings, Error));
    REQUIRE(Settings.Ignored.empty());
    REQUIRE(Resolve({"--vsync", "off"}, {}, Settings, Error));
    REQUIRE(Settings.Ignored.empty());
}

TEST_CASE("Run settings: the frame limiter keeps the lower of the cap and the display's refresh rate", "[RunSettings]")
{
    SECTION("The display does not pace the frames (VSync off): the cap, whatever the display")
    {
        REQUIRE(LimiterFps(60, 60u, false, false) == 60);
        REQUIRE(LimiterFps(144, 60u, false, false) == 144);
        REQUIRE(LimiterFps(0, 60u, false, false) == 0);
    }
    SECTION("The display paces the frames: it paces a cap at or above its refresh rate, a lower cap applies")
    {
        REQUIRE(LimiterFps(60, 60u, true, false) == 0);
        REQUIRE(LimiterFps(144, 60u, true, false) == 0);
        REQUIRE(LimiterFps(59, 60u, true, false) == 59);
        REQUIRE(LimiterFps(0, 60u, true, false) == 0);
    }
    SECTION("In the background the cap is at most 30, whatever the display")
    {
        REQUIRE(LimiterFps(60, 60u, true, true) == RunSettings::BackgroundMaxFps);
        REQUIRE(LimiterFps(0, 60u, true, true) == 30);
        REQUIRE(LimiterFps(20, 60u, true, true) == 20);
        REQUIRE(LimiterFps(144, 60u, false, true) == 30);
        // A display at 30 Hz or below paces the background frames itself
        REQUIRE(LimiterFps(144, 30u, true, true) == 0);
    }
    SECTION("With no known refresh rate (headless, or a monitor that reports 0) the cap applies")
    {
        REQUIRE(LimiterFps(90, std::nullopt, true, false) == 90);
        RunSettings Settings;
        String Error;
        REQUIRE(Resolve({"--null-render", "--max-fps", "90"}, {}, Settings, Error));
        Settings.ResolveFrameCap(144u);
        REQUIRE_FALSE(Settings.MonitorHz.has_value());
        REQUIRE(Resolve({"--max-fps", "90"}, {}, Settings, Error));
        Settings.ResolveFrameCap(0u);
        REQUIRE_FALSE(Settings.MonitorHz.has_value());
    }
}
