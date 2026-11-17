#include "FlingConfig.h"
#include "LaunchOptions.h"
#include "Logger.h"
#include "RunSettings.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>

using namespace Fling;

TEST_CASE("Run settings: the [Engine] keys of an ini file reach the run settings", "[RunSettings][Config]")
{
    Logger::Get().Init();
    const std::filesystem::path Path = std::filesystem::temp_directory_path() / "fling_run_settings_test.ini";
    {
        std::ofstream File(Path, std::ios::trunc);
        File << "[Engine]\nExec=inline\nSimHz=60\nMaxFps=90\nVSync=off\nInterpolation=off\nFrustumCulling="
                "off\nOcclusionCulling=off\n";
    }
    REQUIRE(FlingConfig::Get().LoadConfigFile(Path.string()));

    // As Engine::Startup reads them
    const RunSettingsConfig Config =
        RunSettingsConfig::Read([](const char* t_Key) { return FlingConfig::GetString("Engine", t_Key, ""); });
    RunSettings Settings;
    String Error;
    REQUIRE(RunSettings::Resolve(LaunchOptions{}, Config, Settings, Error));
    REQUIRE(Settings.Describe() ==
            "tick rate 60 Hz (config), exec inline (config), frame cap 90 fps (config), vsync "
            "off (config), interpolation off (config), frustum culling off (config), occlusion culling off (config)");
    std::filesystem::remove(Path);
}
