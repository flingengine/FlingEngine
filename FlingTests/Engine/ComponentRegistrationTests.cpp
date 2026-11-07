#include "ComponentTypeRegistry.h"
#include "Game.h"
#include "JsonArchive.h"
#include "Logger.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

namespace
{
    struct Health
    {
        static constexpr ComponentInfo Component{.Name = "Test.Health"};
        int Value = 100;

        void Serialize(JsonArchive& Ar) { Ar << MakeNVP("value", Value); }
    };

    struct Boss
    {
        static constexpr ComponentInfo Component{.Name = "Test.Boss", .Kind = ComponentKind::Tag};
    };

    /** A cache that is never saved, so it needs no Serialize. */
    struct PathCache
    {
        static constexpr ComponentInfo Component{.Name = "Test.PathCache", .Transient = true};
        std::vector<int> Nodes;
    };

    struct Difficulty
    {
        static constexpr ComponentInfo Component{.Name = "Test.Difficulty", .Kind = ComponentKind::Singleton};
        int Level = 1;

        void Serialize(JsonArchive& Ar) { Ar << MakeNVP("level", Level); }
    };

    struct FrameStats
    {
        static constexpr ComponentInfo Component{
            .Name = "Test.FrameStats", .Kind = ComponentKind::Singleton, .Transient = true};
        int Count = 0;
    };

    struct DamageQueue
    {
        static constexpr ComponentInfo Component{
            .Name = "Test.DamageQueue", .Kind = ComponentKind::Queue, .Transient = true};
        std::vector<int> Requests;
    };

    class RegistrationGame : public Game
    {
    public:
        void Init(Fling::EntityStorage&) override {}
        void RegisterComponents() override
        {
            ComponentTypeRegistry& types = ComponentTypeRegistry::Get();
            types.Register<Health>();
            types.Register<Boss>();
            types.Register<PathCache>();
            types.Register<Difficulty>();
            types.Register<FrameStats>();
            types.Register<DamageQueue>();
        }
        void OnStartGame(Fling::EntityStorage&) override {}
        void Update(Fling::EntityStorage&, float) override {}
        void OnStopGame(Fling::EntityStorage&) override {}
        void Shutdown(Fling::EntityStorage&) override {}
    };

    void QuietLogger()
    {
        Logger::Get().Init();
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }
} // namespace

TEST_CASE("Register<T>() reads the name and kind from ComponentInfo", "[components]")
{
    QuietLogger();
    ComponentTypeRegistry& types = ComponentTypeRegistry::Get();
    types.ResetForTests();

    RegistrationGame game;
    game.RegisterComponents();

    const ComponentTypeInfo* health = types.Find("Test.Health");
    REQUIRE(health != nullptr);
    REQUIRE(health->hash == TypeHashOf<Health>);
    REQUIRE(health->kind == ComponentKind::Table);
    REQUIRE(health->save != nullptr);
    REQUIRE(health->has != nullptr);
    REQUIRE_FALSE(health->IsSingleton());

    const ComponentTypeInfo* boss = types.Find("Test.Boss");
    REQUIRE(boss->kind == ComponentKind::Tag);
    REQUIRE(boss->save != nullptr);

    const ComponentTypeInfo* cache = types.Find("Test.PathCache");
    REQUIRE(cache->transient);
    REQUIRE(cache->save == nullptr);
    REQUIRE(cache->load == nullptr);
    REQUIRE(cache->has != nullptr);

    const ComponentTypeInfo* difficulty = types.Find("Test.Difficulty");
    REQUIRE(difficulty->IsSingleton());
    REQUIRE(difficulty->has == nullptr);
    REQUIRE(difficulty->saveSingleton != nullptr);
    REQUIRE(difficulty->loadSingleton != nullptr);

    const ComponentTypeInfo* stats = types.Find("Test.FrameStats");
    REQUIRE(stats->IsSingleton());
    REQUIRE(stats->hasSingleton != nullptr);
    REQUIRE(stats->saveSingleton == nullptr);

    REQUIRE(types.Find("Test.DamageQueue")->kind == ComponentKind::Queue);

    // The name is the key, so registering it again fails
    REQUIRE_FALSE(types.Register<Health>());

    types.ResetForTests();
}

TEST_CASE("Saved singletons and tags round-trip through the play snapshot", "[components][world]")
{
    QuietLogger();
    ComponentTypeRegistry::Get().ResetForTests();

    RegistrationGame TestGame;
    World TestWorld(&TestGame);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();
    REQUIRE(TestWorld.Init());

    const Fling::Entity boss = Reg.Create();
    Reg.Add<Health>(boss, Health{40});
    Reg.Add<Boss>(boss);
    Reg.Add<PathCache>(boss);
    Reg.SetSingleton<Difficulty>(Difficulty{5});
    Reg.SetSingleton<FrameStats>(FrameStats{1});

    TestWorld.RequestGameStart();
    TestWorld.Update(0.016f);

    // Play changes the state
    Reg.Singleton<Difficulty>().Level = 9;
    Reg.Singleton<FrameStats>().Count = 7;
    Reg.View<Health>().Each([](auto, Health& h) { h.Value = 1; });

    TestWorld.RequestGameStop();
    TestWorld.Update(0.016f);

    // A saved singleton comes back from the snapshot. A transient one keeps its value.
    REQUIRE(Reg.Singleton<Difficulty>().Level == 5);
    REQUIRE(Reg.Singleton<FrameStats>().Count == 7);

    // The entity comes back with its saved components. The transient cache is not saved.
    REQUIRE(Reg.View<Boss>().Size() == 1);
    const Fling::Entity restored = *Reg.View<Boss>().begin();
    REQUIRE(Reg.Get<Health>(restored).Value == 40);
    REQUIRE_FALSE(Reg.Has<PathCache>(restored));

    TestWorld.Shutdown();
    ComponentTypeRegistry::Get().ResetForTests();
}
