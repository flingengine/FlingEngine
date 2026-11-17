// Tests of the live run settings (RunControls): what the GPU Options window changes, and
// what the window system reports. The world comes from the threaded world harness, in
// Direct mode.

#include "RunControls.h"
#include "ThreadedWorldHarness.h"

#include <catch2/catch_all.hpp>

using namespace ThreadedTest;

namespace
{
    /** The settings of a windowed run on a 60 Hz monitor with no cap set: the cap follows the monitor. */
    RunSettings OnMonitor(const uint32 t_Hz)
    {
        RunSettings Settings;
        Settings.ResolveFrameCap(t_Hz);
        return Settings;
    }
} // namespace

TEST_CASE("Run controls: the tick meter counts the ticks of the last whole second", "[RunControls]")
{
    RateMeter Meter;
    REQUIRE(Meter.PerSecond() == 0.0);
    Meter.Add(1'000'000'000, 10);
    Meter.Add(1'500'000'000, 25);
    REQUIRE(Meter.PerSecond() == 0.0);
    Meter.Add(2'000'000'000, 40);
    REQUIRE(Meter.PerSecond() == Catch::Approx(30.0));
    Meter.Add(3'250'000'000, 115);
    REQUIRE(Meter.PerSecond() == Catch::Approx(60.0));
}

TEST_CASE("Run controls: the live values start from the startup settings, which do not change", "[RunControls]")
{
    ThreadedWorldHarness Harness(HarnessOptions{});
    RunSettings Settings = OnMonitor(60);
    Settings.VSync = false;
    Settings.FrustumCulling = false;
    Settings.OcclusionCulling = false;
    const RunSettings Startup = Settings;
    RunControls Controls(Settings, Harness.GetWorld());
    REQUIRE_FALSE(Controls.VSync());
    REQUIRE_FALSE(Controls.FrustumCulling());
    REQUIRE_FALSE(Controls.OcclusionCulling());
    REQUIRE(Controls.MaxFps() == 60);
    REQUIRE(Controls.MonitorHz() == 60u);

    String Error;
    Controls.SetVSync(true);
    REQUIRE(Controls.SetMaxFps(90, Error));
    Controls.SetInterpolation(false);
    Controls.SetFrustumCulling(true);
    Controls.SetOcclusionCulling(true);
    REQUIRE(Controls.VSync());
    REQUIRE(Controls.MaxFps() == 90);
    REQUIRE(Controls.FrustumCulling());
    REQUIRE(Controls.OcclusionCulling());
    REQUIRE(Settings.Describe() == Startup.Describe());
    REQUIRE_FALSE(Settings.VSync);
    REQUIRE_FALSE(Settings.FrustumCulling);
    REQUIRE_FALSE(Settings.OcclusionCulling);
}

TEST_CASE("Run controls: the frame cap takes 0 to 1000, and an error is cleared by the next good value",
          "[RunControls]")
{
    ThreadedWorldHarness Harness(HarnessOptions{});
    RunControls Controls(OnMonitor(60), Harness.GetWorld());
    String Error;
    REQUIRE_FALSE(Controls.SetMaxFps(1001, Error));
    REQUIRE_THAT(Error, Catch::Matchers::ContainsSubstring("0 (no cap) or from 1 to 1000"));
    REQUIRE(Controls.MaxFps() == 60);
    REQUIRE(Controls.SetMaxFps(0, Error));
    REQUIRE(Error.empty());
    REQUIRE(Controls.MaxFps() == 0);
    REQUIRE(Controls.SetMaxFps(1000, Error));
}

TEST_CASE("Run controls: interpolation lives in the world, and the controls show the world's value", "[RunControls]")
{
    ThreadedWorldHarness Harness(HarnessOptions{});
    RunControls Controls(RunSettings{}, Harness.GetWorld());
    REQUIRE(Controls.Interpolation());
    Controls.SetInterpolation(false);
    REQUIRE_FALSE(Harness.GetWorld().GetInterpolation());
    Harness.GetWorld().SetInterpolation(true);
    REQUIRE(Controls.Interpolation());
}

TEST_CASE("Run controls: a cap from the monitor follows the monitor the window moves to; a set cap stays",
          "[RunControls]")
{
    ThreadedWorldHarness Harness(HarnessOptions{});
    RunControls Controls(OnMonitor(60), Harness.GetWorld());
    Controls.SetMonitorHz(144u);
    REQUIRE(Controls.MaxFps() == 144);
    REQUIRE(Controls.LimiterFps(true) == 0);

    // A monitor that reports nothing keeps the cap, and the limiter no longer knows the refresh rate
    Controls.SetMonitorHz(0u);
    REQUIRE_FALSE(Controls.MonitorHz().has_value());
    REQUIRE(Controls.MaxFps() == 144);
    REQUIRE(Controls.LimiterFps(true) == 144);

    String Error;
    REQUIRE(Controls.SetMaxFps(100, Error));
    Controls.SetMonitorHz(240u);
    REQUIRE(Controls.MaxFps() == 100);
    REQUIRE(Controls.LimiterFps(true) == 100);

    // A cap from the config file never followed the monitor
    RunSettings FromConfig;
    FromConfig.MaxFps = 75;
    FromConfig.MaxFpsSource = SettingSource::Config;
    FromConfig.ResolveFrameCap(60u);
    RunControls Configured(FromConfig, Harness.GetWorld());
    Configured.SetMonitorHz(144u);
    REQUIRE(Configured.MaxFps() == 75);
}

TEST_CASE("Run controls: the limiter text says what the limiter does", "[RunControls]")
{
    ThreadedWorldHarness Harness(HarnessOptions{});
    RunControls Controls(OnMonitor(60), Harness.GetWorld());
    String Error;

    // The default: VSync on, cap = refresh rate
    REQUIRE(Controls.DescribeLimiter(true) == "The display paces the frames");
    REQUIRE(Controls.DescribeLimiter(false) == "The limiter keeps 60 fps");
    REQUIRE(Controls.SetMaxFps(40, Error));
    REQUIRE(Controls.DescribeLimiter(true) == "The limiter keeps 40 fps");
    REQUIRE(Controls.SetMaxFps(0, Error));
    REQUIRE(Controls.DescribeLimiter(false) == "No cap");

    Controls.SetInBackground(true);
    REQUIRE(Controls.DescribeLimiter(false) == "In the background: at most 30 fps");
    REQUIRE(Controls.DescribeLimiter(true) == "In the background: at most 30 fps");
    REQUIRE(Controls.SetMaxFps(20, Error));
    REQUIRE(Controls.DescribeLimiter(false) == "The limiter keeps 20 fps");
    REQUIRE(Controls.SetMaxFps(30, Error));
    REQUIRE(Controls.DescribeLimiter(false) == "The limiter keeps 30 fps");
}

TEST_CASE("Run controls: the measured rates say when the background cap applies", "[RunControls]")
{
    ThreadedWorldHarness Harness(HarnessOptions{});
    RunControls Controls(OnMonitor(60), Harness.GetWorld());
    REQUIRE(Controls.DescribeMeasured(59.6) == "Measured: 60 frames a second, 0.0 ticks a second");
    Controls.SetInBackground(true);
    REQUIRE(Controls.DescribeMeasured(30.0) == "Measured: 30 frames a second, 0.0 ticks a second (window in the "
                                               "background: at most 30 frames a second)");
}
