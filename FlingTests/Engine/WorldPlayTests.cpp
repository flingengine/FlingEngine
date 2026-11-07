#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Game.h"
#include "Logger.h"
#include "Strings/String.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <vector>

using Fling::String;
using Fling::StringView;

namespace
{
    /** Spawns entities when play starts and records the calls that the world makes */
    class PlayTestGame : public Fling::Game
    {
    public:
        void Init(Fling::EntityStorage&) override { Calls.push_back("Init"); }

        void Register(Fling::ModuleRegistration& t_Modules) override
        {
            Fling::RegisterEngine(t_Modules);
        }

        void OnStartGame(Fling::EntityStorage& t_Reg) override
        {
            Calls.push_back("OnStartGame");
            for (int i = 0; i < SpawnCount; ++i)
            {
                t_Reg.Add<Fling::NameComponent>(t_Reg.Create()).Name = Fling::Name("Spawned");
            }
        }

        void Update(Fling::EntityStorage&, float) override
        {
            ++Updates;
            if (QuitOnUpdate)
            {
                m_WantsToQuit = true;
            }
        }

        void OnStopGame(Fling::EntityStorage& t_Reg) override
        {
            Calls.push_back("OnStopGame");
            // The entities that play made still exist during this call
            EntitiesAtStop = static_cast<int>(t_Reg.Count<Fling::NameComponent>());
        }

        void Shutdown(Fling::EntityStorage&) override { Calls.push_back("Shutdown"); }

        std::vector<String> Calls;
        int SpawnCount = 3;
        int Updates = 0;
        int EntitiesAtStop = -1;
        bool QuitOnUpdate = false;
    };

    void InitTestLogger()
    {
        Fling::Logger::Get().Init();
        Fling::Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Fling::Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    /** Makes one named entity with a transform at t_X */
    Fling::Entity MakeAuthoredEntity(Fling::EntityStorage& t_Reg, const StringView t_Name, float t_X)
    {
        const Fling::Entity Entity = t_Reg.Create();
        t_Reg.Add<Fling::NameComponent>(Entity).Name = Fling::Name(t_Name);
        t_Reg.Add<Fling::Transform>(Entity).Position = glm::vec3(t_X, 0.0f, 0.0f);
        return Entity;
    }

    int CountNamed(Fling::EntityStorage& t_Reg, const StringView t_Name)
    {
        int Count = 0;
        t_Reg.View<Fling::NameComponent>().Each(
            [&](auto, Fling::NameComponent& t_NameComp)
            {
                if (t_NameComp.Name.ToView() == t_Name)
                {
                    ++Count;
                }
            });
        return Count;
    }
} // namespace

TEST_CASE("World play sessions reset the world when play stops", "[world][play]")
{
    using namespace Fling;
    InitTestLogger();

    PlayTestGame TestGame;
    World TestWorld(&TestGame);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();
    TestWorld.Init();
    REQUIRE(TestWorld.IsReadyForPlay());

    MakeAuthoredEntity(Reg, "Authored", 2.0f);
    REQUIRE(Reg.Count<NameComponent>() == 1);

    SECTION("Requests run on the next update")
    {
        TestWorld.RequestGameStart();
        REQUIRE(TestWorld.HasPendingPlayRequest());
        REQUIRE(TestWorld.IsReadyForPlay());

        TestWorld.Update(0.016f);
        REQUIRE(TestWorld.IsPlaying());
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());
        REQUIRE(TestGame.Updates == 1);

        TestWorld.RequestGameStop();
        REQUIRE(TestWorld.IsPlaying());
        TestWorld.Update(0.016f);
        REQUIRE(TestWorld.IsReadyForPlay());
        // No game update after the stop
        REQUIRE(TestGame.Updates == 1);
    }

    SECTION("Stop removes spawned entities and restores authored values")
    {
        TestWorld.RequestGameStart();
        TestWorld.Update(0.016f);
        REQUIRE(CountNamed(Reg, "Spawned") == 3);

        // Play changes an authored entity
        Reg.View<NameComponent, Transform>().Each([](auto, NameComponent&, Transform& t_Trans)
                                                  { t_Trans.Position = glm::vec3(99.0f, 0.0f, 0.0f); });

        TestWorld.RequestGameStop();
        TestWorld.Update(0.016f);

        REQUIRE(TestGame.EntitiesAtStop == 4);
        REQUIRE(CountNamed(Reg, "Spawned") == 0);
        REQUIRE(CountNamed(Reg, "Authored") == 1);
        Reg.View<NameComponent, Transform>().Each([](auto, NameComponent&, Transform& t_Trans)
                                                  { REQUIRE(t_Trans.Position.x == Catch::Approx(2.0f)); });
    }

    SECTION("Play can start and stop many times")
    {
        for (int i = 0; i < 3; ++i)
        {
            TestWorld.RequestGameStart();
            TestWorld.Update(0.016f);
            REQUIRE(CountNamed(Reg, "Spawned") == 3);

            TestWorld.RequestGameStop();
            TestWorld.Update(0.016f);
            REQUIRE(Reg.Count<NameComponent>() == 1);
        }

        const std::vector<String> Expected = {
            "Init",
            "OnStartGame",
            "OnStopGame",
            "OnStartGame",
            "OnStopGame",
            "OnStartGame",
            "OnStopGame",
        };
        REQUIRE(TestGame.Calls == Expected);
    }

    SECTION("Stop before the start runs cancels the start")
    {
        TestWorld.RequestGameStart();
        TestWorld.RequestGameStop();
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());

        TestWorld.Update(0.016f);
        REQUIRE(TestWorld.IsReadyForPlay());
        REQUIRE(TestGame.Calls == std::vector<String>{"Init"});
    }

    SECTION("Requests in the wrong state have no effect")
    {
        TestWorld.RequestGameStop();
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());

        TestWorld.RequestGameStart();
        TestWorld.Update(0.016f);
        TestWorld.RequestGameStart();
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());
        REQUIRE(TestWorld.IsPlaying());
    }

    SECTION("A game quit stops play when SetGameQuitStopsPlay is true")
    {
        TestWorld.SetGameQuitStopsPlay(true);
        TestGame.QuitOnUpdate = true;

        TestWorld.RequestGameStart();
        TestWorld.Update(0.016f);
        REQUIRE_FALSE(TestWorld.ShouldQuit());
        REQUIRE_FALSE(TestGame.WantsToQuit());
        REQUIRE(TestWorld.HasPendingPlayRequest());

        TestWorld.Update(0.016f);
        REQUIRE(TestWorld.IsReadyForPlay());
        REQUIRE(CountNamed(Reg, "Spawned") == 0);
    }

    SECTION("A game quit quits when SetGameQuitStopsPlay is false")
    {
        TestGame.QuitOnUpdate = true;

        TestWorld.RequestGameStart();
        TestWorld.Update(0.016f);
        REQUIRE(TestWorld.ShouldQuit());
        REQUIRE(TestWorld.IsPlaying());
    }

    SECTION("Shutdown during play stops the game once and does not reset")
    {
        TestWorld.RequestGameStart();
        TestWorld.Update(0.016f);
        TestWorld.Shutdown();

        const std::vector<String> Expected = {"Init", "OnStartGame", "OnStopGame", "Shutdown"};
        REQUIRE(TestGame.Calls == Expected);
        REQUIRE(CountNamed(Reg, "Spawned") == 3);
    }

    SECTION("Shutdown when not playing does not call OnStopGame")
    {
        TestWorld.Shutdown();
        const std::vector<String> Expected = {"Init", "Shutdown"};
        REQUIRE(TestGame.Calls == Expected);
    }

}
