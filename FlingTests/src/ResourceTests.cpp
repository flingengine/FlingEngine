#include "catch2/catch.hpp"

#include "pch.h"
#include "Singleton.hpp"
#include "FlingConfig.h"
#include "ResourceManager.h"

TEST_CASE("Engine Config File", "[resource]")
{
    SECTION("valid Config")
    {
        using namespace Fling;
        // Logger HAS to be initalized first
        Logger::Get().Init();
        ResourceManager::Get().Init();
        FlingConfig::Get().Init();
        
        // Load a test config
        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile("Config/EngineConf.ini");
        REQUIRE(ConfigLoaded);

        ResourceManager::Get().Shutdown();
        Logger::Get().Shutdown();
        FlingConfig::Get().Shutdown();
    }
}