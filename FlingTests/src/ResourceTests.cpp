#include "catch2/catch.hpp"

#include "pch.h"
#include "Singleton.hpp"
#include "FlingConfig.h"
#include "ResourceManager.h"

// @see TestConf.ini

TEST_CASE("Engine Config File", "[resource]")
{
    using namespace Fling;
    // Logger HAS to be initalized first
    Logger::Get().Init();
    ResourceManager::Get().Init();
    FlingConfig::Get().Init();

    SECTION("Valid Config")
    {
        // Load a test config
        bool ConfigLoaded = FlingConfig::Get().LoadConfigFile(FlingPaths::EngineConfigDir() + "/TestConf.ini");
        REQUIRE(ConfigLoaded);
    }

    SECTION("Read False Bool")
    {
        bool FalseFlag = FlingConfig::Get().GetBool("TestRead", "falseFlag");
        REQUIRE_FALSE(FalseFlag);
    }

    SECTION("Read True Bool")
    {
        bool TrueFlag = FlingConfig::Get().GetBool("TestRead", "trueFlag");
        REQUIRE(TrueFlag);
    }

    SECTION("Positive Num Test")
    {
        int Num = FlingConfig::Get().GetInt("TestRead", "NumberTest");
        REQUIRE(Num == 42);
    }

    SECTION("Negative Num Test")
    {
        int Num = FlingConfig::Get().GetInt("TestRead", "NegNumTest");
        REQUIRE(Num == -77);
    }

    SECTION("String Test")
    {
        std::string Words = FlingConfig::Get().GetString("TestRead", "Words");
        REQUIRE(Words == "Billy Bob Joe");
    }


    ResourceManager::Get().Shutdown();
    Logger::Get().Shutdown();
    FlingConfig::Get().Shutdown();
}