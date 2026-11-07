#include "Ecs/ModuleRegistration.h"
#include "Ecs/TypeRegistry.h"
#include "Game.h"
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
    };

    struct Boss
    {
        static constexpr ComponentInfo Component{.Name = "Test.Boss", .Kind = ComponentKind::Tag};
    };

    /** A cache that is never saved, so it reflects no fields. */
    struct PathCache
    {
        static constexpr ComponentInfo Component{.Name = "Test.PathCache", .Transient = true};
        std::vector<int> Nodes;
    };

    struct Difficulty
    {
        static constexpr ComponentInfo Component{.Name = "Test.Difficulty", .Kind = ComponentKind::Singleton};
        int Level = 1;
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

    void RegisterTestTypes(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<Health>([](TypeBuilder<Health>& t_Type) { t_Type.Field("value", &Health::Value); });
        t_Modules.Component<Boss>();
        t_Modules.Component<PathCache>();
        t_Modules.Component<Difficulty>([](TypeBuilder<Difficulty>& t_Type)
                                        { t_Type.Field("level", &Difficulty::Level); });
        t_Modules.Component<FrameStats>();
        t_Modules.Component<DamageQueue>();
    }

    class RegistrationGame : public Game
    {
    public:
        void Init(Fling::EntityStorage&) override {}
        void Register(ModuleRegistration& t_Modules) override { RegisterTestTypes(t_Modules); }
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

TEST_CASE("Component<T>() reads the name and kind from ComponentInfo", "[components]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterTestTypes(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);

    const TypeDesc* health = types.Find("Test.Health");
    REQUIRE(health != nullptr);
    REQUIRE(health->Hash == TypeHashOf<Health>);
    REQUIRE(types.Find<Health>() == health);
    REQUIRE(health->Kind == ComponentKind::Table);
    REQUIRE(health->IsSaved());
    REQUIRE(health->Has != nullptr);
    REQUIRE(health->Get != nullptr);
    REQUIRE(health->Fields.size() == 1);
    REQUIRE_FALSE(health->IsSingleton());

    const TypeDesc* boss = types.Find("Test.Boss");
    REQUIRE(boss->Kind == ComponentKind::Tag);
    REQUIRE(boss->IsSaved());
    REQUIRE(boss->Get == nullptr);

    const TypeDesc* cache = types.Find("Test.PathCache");
    REQUIRE(cache->Transient);
    REQUIRE_FALSE(cache->IsSaved());
    REQUIRE(cache->Has != nullptr);

    const TypeDesc* difficulty = types.Find("Test.Difficulty");
    REQUIRE(difficulty->IsSingleton());
    REQUIRE(difficulty->Has == nullptr);
    REQUIRE(difficulty->GetSingleton != nullptr);
    REQUIRE(difficulty->EmplaceSingleton != nullptr);

    const TypeDesc* stats = types.Find("Test.FrameStats");
    REQUIRE(stats->IsSingleton());
    REQUIRE_FALSE(stats->IsSaved());

    REQUIRE(types.Find("Test.DamageQueue")->Kind == ComponentKind::Queue);

    // The name is the key, so adding it again is ignored
    modules.Component<Health>([](TypeBuilder<Health>&) {});
    REQUIRE(modules.Types().size() == 6);
    REQUIRE(TypeRegistry::Build(modules).Find<Health>()->Fields.size() == 1);
}

TEST_CASE("Two type registries built in one test stay separate", "[components]")
{
    QuietLogger();
    SystemRegistry systemsA;
    ModuleRegistration modulesA(systemsA);
    modulesA.Component<Health>([](TypeBuilder<Health>& t_Type) { t_Type.Field("value", &Health::Value); });

    SystemRegistry systemsB;
    ModuleRegistration modulesB(systemsB);
    modulesB.Component<Boss>();

    const TypeRegistry a = TypeRegistry::Build(modulesA);
    const TypeRegistry b = TypeRegistry::Build(modulesB);
    REQUIRE(a.Find<Health>() != nullptr);
    REQUIRE(a.Find<Boss>() == nullptr);
    REQUIRE(b.Find<Boss>() != nullptr);
    REQUIRE(b.Find<Health>() == nullptr);
}

TEST_CASE("Saved singletons and tags round-trip through the play snapshot", "[components][world]")
{
    QuietLogger();

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
}
