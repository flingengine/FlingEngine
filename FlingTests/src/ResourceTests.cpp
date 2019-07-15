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
        ResourceManager::Get().Init();
        Logger::Get().Init();
        FlingConfig::Get().Init();
        
        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile("EngineConf.ini");
        REQUIRE(ConfigLoaded);

        ResourceManager::Get().Shutdown();
        Logger::Get().Shutdown();
        FlingConfig::Get().Shutdown();
    }
}