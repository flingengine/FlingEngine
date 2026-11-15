#include "Components/AppRequests.h"
#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SystemBase.h"
#include "Game.h"
#include "Logger.h"
#include "Strings/String.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>

using Fling::String;
using Fling::StringView;

namespace
{
    /** The tick rate these tests are written for: OneTickFrame is a little longer than one tick */
    constexpr uint32 TestSimHz = 60;

    /** A frame a little longer than one fixed step (1/60 s): each Update runs one tick */
    constexpr float OneTickFrame = 0.017f;

    /** What the play system saw, and whether it asks to quit. */
    struct PlayTicks
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.PlayTicks",
                                                        .Kind = Fling::ComponentKind::Singleton,
                                                        .Transient = true,
                                                        .Rollback = Fling::RollbackClass::Keep};
        int Count = 0;
        bool QuitEachTick = false;
    };

    /** Counts the play ticks, and asks the application to quit on each one when told to. */
    class PlayTickSystem final
        : public Fling::System<PlayTickSystem,
                               Fling::Access<Fling::Write<PlayTicks>, Fling::Append<Fling::AppRequests>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Test.PlayTicks", .In = Fling::Order::Input};

        void OnCreate(Setup& setup) const { setup.AddSingleton<PlayTicks>(); }

        void OnDestroy(Setup& setup) const { setup.RemoveSingleton<PlayTicks>(); }

        void FixedUpdate(Context& ctx) const
        {
            PlayTicks& Ticks = ctx.Singleton<PlayTicks>();
            ++Ticks.Count;
            if (Ticks.QuitEachTick)
            {
                ctx.Queue<Fling::AppRequests>().Push(Fling::AppRequest::Quit);
            }
        }
    };

    /** Spawns entities when play starts and records the calls that the world makes */
    class PlayTestGame : public Fling::Game
    {
    public:
        void Init(Fling::EntityStorage&) override { Calls.push_back("Init"); }

        void Register(Fling::ModuleRegistration& t_Modules) override
        {
            Fling::RegisterEngine(t_Modules);
            t_Modules.System<PlayTickSystem>();
        }

        void OnStartGame(Fling::EntityStorage& t_Reg) override
        {
            Calls.push_back("OnStartGame");
            for (int i = 0; i < SpawnCount; ++i)
            {
                t_Reg.Add<Fling::NameComponent>(t_Reg.Create()).Name = Fling::Name("Spawned");
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
        int EntitiesAtStop = -1;
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

    const auto TestGameOwner = std::make_shared<PlayTestGame>();
    PlayTestGame& TestGame = *TestGameOwner;
    World TestWorld(TestGameOwner);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();
    // Play on this thread, at the rate that OneTickFrame is made for
    TestWorld.SetExecMode(ExecMode::Direct);
    TestWorld.SetSystemRunnerConfig(SystemRunnerConfig{.SimHz = TestSimHz});
    TestWorld.Init();
    REQUIRE(TestWorld.IsReadyForPlay());

    MakeAuthoredEntity(Reg, "Authored", 2.0f);
    REQUIRE(Reg.Count<NameComponent>() == 1);

    SECTION("Requests run on the next update")
    {
        TestWorld.RequestGameStart();
        REQUIRE(TestWorld.HasPendingPlayRequest());
        REQUIRE(TestWorld.IsReadyForPlay());

        TestWorld.Update(OneTickFrame);
        REQUIRE(TestWorld.IsPlaying());
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());
        REQUIRE(Reg.Singleton<PlayTicks>().Count == 1);

        TestWorld.RequestGameStop();
        REQUIRE(TestWorld.IsPlaying());
        TestWorld.Update(OneTickFrame);
        REQUIRE(TestWorld.IsReadyForPlay());
        // No play tick after the stop
        REQUIRE(Reg.Singleton<PlayTicks>().Count == 1);
    }

    SECTION("Stop removes spawned entities and restores authored values")
    {
        TestWorld.RequestGameStart();
        TestWorld.Update(OneTickFrame);
        REQUIRE(CountNamed(Reg, "Spawned") == 3);

        // Play changes an authored entity
        Reg.View<NameComponent, Transform>().Each([](auto, NameComponent&, Transform& t_Trans)
                                                  { t_Trans.Position = glm::vec3(99.0f, 0.0f, 0.0f); });

        TestWorld.RequestGameStop();
        TestWorld.Update(OneTickFrame);

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
            TestWorld.Update(OneTickFrame);
            REQUIRE(CountNamed(Reg, "Spawned") == 3);

            TestWorld.RequestGameStop();
            TestWorld.Update(OneTickFrame);
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

        TestWorld.Update(OneTickFrame);
        REQUIRE(TestWorld.IsReadyForPlay());
        REQUIRE(TestGame.Calls == std::vector<String>{"Init"});
    }

    SECTION("Requests in the wrong state have no effect")
    {
        TestWorld.RequestGameStop();
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());

        TestWorld.RequestGameStart();
        TestWorld.Update(OneTickFrame);
        TestWorld.RequestGameStart();
        REQUIRE_FALSE(TestWorld.HasPendingPlayRequest());
        REQUIRE(TestWorld.IsPlaying());
    }

    SECTION("A game quit stops play when SetGameQuitStopsPlay is true")
    {
        TestWorld.SetGameQuitStopsPlay(true);
        Reg.Singleton<PlayTicks>().QuitEachTick = true;

        TestWorld.RequestGameStart();
        TestWorld.Update(OneTickFrame);
        REQUIRE_FALSE(TestWorld.ShouldQuit());
        REQUIRE(TestWorld.HasPendingPlayRequest());

        // The session ended at the Quit tick: the next frame runs no play tick, then play stops
        TestWorld.Update(OneTickFrame);
        REQUIRE(TestWorld.IsReadyForPlay());
        REQUIRE(Reg.Singleton<PlayTicks>().Count == 1);
        REQUIRE(CountNamed(Reg, "Spawned") == 0);
    }

    SECTION("A game quit quits when SetGameQuitStopsPlay is false")
    {
        Reg.Singleton<PlayTicks>().QuitEachTick = true;

        TestWorld.RequestGameStart();
        TestWorld.Update(OneTickFrame);
        REQUIRE(TestWorld.ShouldQuit());
        REQUIRE(TestWorld.IsPlaying());

        // No tick runs after the Quit tick
        TestWorld.Update(OneTickFrame);
        REQUIRE(Reg.Singleton<PlayTicks>().Count == 1);
        REQUIRE(TestWorld.HasRunLastTick());
    }

    SECTION("Shutdown during play stops the game once and does not reset")
    {
        TestWorld.RequestGameStart();
        TestWorld.Update(OneTickFrame);
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
