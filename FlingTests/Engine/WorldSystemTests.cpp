#include "ComponentTypeRegistry.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "Game.h"
#include "Logger.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <string>

// Fixture systems that exist only for these tests (Plans/ecs-systems.md section 3.3).

using namespace Fling;

namespace
{
    /** What the systems saw. One singleton, so the test can read it from the registry. */
    struct Counters
    {
        static constexpr ComponentInfo Component{
            .Name = "Test.WorldCounters", .Kind = ComponentKind::Singleton, .Transient = true};
        int Ticks = 0;
        int EditTicks = 0;
        int PlayBegan = 0;
        int PlayEnded = 0;
        uint64 LastTick = 0;
    };

    struct FrameCount
    {
        static constexpr ComponentInfo Component{.Name = "Test.WorldFrameCount",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
        int Frames = 0;
    };

    /** Counts play ticks. Owns Counters. */
    class PlayCounterSystem final : public System<PlayCounterSystem, Access<Read<SimTime>, Write<Counters>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.PlayCounter", .Phase = Phases::Simulation};

        void OnCreate(Setup& setup) const { setup.AddSingleton<Counters>(); }

        void OnDestroy(Setup& setup) const { setup.RemoveSingleton<Counters>(); }

        void OnPlayBegin(Context& ctx) const { ctx.Singleton<Counters>().PlayBegan++; }

        void OnPlayEnd(Context& ctx) const { ctx.Singleton<Counters>().PlayEnded++; }

        void FixedUpdate(Context& ctx) const
        {
            Counters& counters = ctx.Singleton<Counters>();
            counters.Ticks++;
            counters.LastTick = ctx.Singleton<const SimTime>().Tick;
        }
    };

    /** Runs only when the world is not playing. */
    class EditCounterSystem final : public System<EditCounterSystem, Access<Write<Counters>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.EditCounter", .Phase = Phases::Input, .RunIn = WorldRole::Edit};

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Counters>().EditTicks++; }
    };

    /** A frame system that runs in every role. */
    class FrameCounterSystem final : public System<FrameCounterSystem, Access<Write<FrameCount>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Test.FrameCounter", .Phase = Phases::PostSimulation, .RunIn = WorldRole::All};

        void OnCreate(Setup& setup) const { setup.AddSingleton<FrameCount>(); }

        void Update(Context& ctx) const { ctx.Singleton<FrameCount>().Frames++; }
    };

    /** A second writer of Counters in the same phase, with no order: a schedule error. */
    class ConflictingSystem final : public System<ConflictingSystem, Access<Write<Counters>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Conflicting", .Phase = Phases::Simulation};

        void FixedUpdate(Context&) const {}
    };

    class SystemsGame : public Game
    {
    public:
        void Init(Fling::EntityStorage&) override {}
        void OnStartGame(Fling::EntityStorage&) override {}
        void Update(Fling::EntityStorage&, float) override { ++Updates; }
        void OnStopGame(Fling::EntityStorage&) override {}
        void Shutdown(Fling::EntityStorage&) override {}

        void RegisterSystems(SystemRegistry& t_Systems) override
        {
            t_Systems.Add<PlayCounterSystem>();
            t_Systems.Add<EditCounterSystem>();
            t_Systems.Add<FrameCounterSystem>();
            if (AddConflict)
            {
                t_Systems.Add<ConflictingSystem>();
            }
        }

        bool AddConflict = false;
        int Updates = 0;
    };

    void QuietLogger()
    {
        Logger::Get().Init();
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }
} // namespace

TEST_CASE("The world runs its systems by role and calls the play hooks", "[world][ecs]")
{
    QuietLogger();
    ComponentTypeRegistry::Get().ResetForTests();

    SystemsGame TestGame;
    World TestWorld(&TestGame);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();
    TestWorld.SetSystemRunnerConfig(SystemRunnerConfig{.OneTickPerFrame = true});
    REQUIRE(TestWorld.Init());
    REQUIRE(TestWorld.GetSchedule() != nullptr);
    REQUIRE(TestWorld.GetScheduleErrors().empty());
    REQUIRE(TestWorld.GetSystems().Size() == 3);

    // OnCreate ran before the first update
    REQUIRE(Reg.TryGetSingleton<Counters>() != nullptr);

    // Not playing: the Edit role runs, the Play role does not, and Game::Update does not
    TestWorld.Update(0.016f);
    REQUIRE(Reg.Singleton<Counters>().EditTicks == 1);
    REQUIRE(Reg.Singleton<Counters>().Ticks == 0);
    REQUIRE(Reg.Singleton<FrameCount>().Frames == 1);
    REQUIRE(TestGame.Updates == 0);

    // Play: OnPlayBegin, then the Play role and the game update
    TestWorld.RequestGameStart();
    TestWorld.Update(0.016f);
    REQUIRE(Reg.Singleton<Counters>().PlayBegan == 1);
    REQUIRE(Reg.Singleton<Counters>().Ticks == 1);
    REQUIRE(Reg.Singleton<Counters>().EditTicks == 1);
    REQUIRE(Reg.Singleton<FrameCount>().Frames == 2);
    REQUIRE(TestGame.Updates == 1);

    // The simulation clock counts every tick, in both roles
    REQUIRE(Reg.Singleton<Counters>().LastTick == 2);

    // Stop: OnPlayEnd. The singletons are in the registry context, so the snapshot restore keeps them
    TestWorld.RequestGameStop();
    TestWorld.Update(0.016f);
    REQUIRE(Reg.Singleton<Counters>().PlayEnded == 1);
    REQUIRE(Reg.Singleton<Counters>().EditTicks == 2);

    // Shutdown: OnDestroy removes the singletons
    TestWorld.Shutdown();
    REQUIRE(Reg.TryGetSingleton<Counters>() == nullptr);

    ComponentTypeRegistry::Get().ResetForTests();
}

TEST_CASE("A world with a bad schedule does not start", "[world][ecs]")
{
    QuietLogger();
    ComponentTypeRegistry::Get().ResetForTests();

    SystemsGame TestGame;
    TestGame.AddConflict = true;
    World TestWorld(&TestGame);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();

    REQUIRE_FALSE(TestWorld.Init());
    REQUIRE_FALSE(TestWorld.IsReadyForPlay());
    REQUIRE(TestWorld.GetSchedule() == nullptr);
    REQUIRE(TestWorld.GetScheduleErrors().find("Test.Conflicting and Test.PlayCounter both write Test.WorldCounters") !=
            std::string::npos);

    // No system ran OnCreate
    REQUIRE(Reg.TryGetSingleton<Counters>() == nullptr);

    TestWorld.Shutdown();
    ComponentTypeRegistry::Get().ResetForTests();
}
