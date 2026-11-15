#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SystemBase.h"
#include "Game.h"
#include "Json.h"
#include "Logger.h"
#include "Strings/String.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>

using Fling::String;

namespace
{
    constexpr uint32 TestSimHz = 60;

    /** A frame a little longer than one fixed step (1/60 s): each Update runs one tick */
    constexpr float OneTickFrame = 0.017f;

    /**
     * Simulation state that a level does not save (Transient) but a rollback restores: the
     * state dump must show it, as the state hash covers it.
     */
    struct DumpCounter
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.DumpCounter",
                                                        .Kind = Fling::ComponentKind::Singleton,
                                                        .Transient = true,
                                                        .Rollback = Fling::RollbackClass::Restore};
        int Ticks = 0;
    };

    /** A setting that is neither saved nor rolled back: the state dump leaves it out. */
    struct DumpScratch
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Test.DumpScratch",
                                                        .Kind = Fling::ComponentKind::Singleton,
                                                        .Transient = true,
                                                        .Rollback = Fling::RollbackClass::Keep};
        int Value = 7;
    };

    /** Counts the ticks and moves every transform one unit along x each tick. */
    class DumpTickSystem final
        : public Fling::System<DumpTickSystem, Fling::Access<Fling::Write<DumpCounter>, Fling::Write<Fling::Transform>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Test.DumpTicks", .In = Fling::Order::Input};

        void OnCreate(Setup& setup) const { setup.AddSingleton<DumpCounter>(); }

        void OnDestroy(Setup& setup) const { setup.RemoveSingleton<DumpCounter>(); }

        void FixedUpdate(Context& ctx) const
        {
            ++ctx.Singleton<DumpCounter>().Ticks;
            for (auto [Entity, Trans] : ctx.Query<Fling::Transform>())
            {
                Trans.Position.x += 1.0f;
            }
        }
    };

    class DumpTestGame : public Fling::Game
    {
    public:
        void Init(Fling::EntityStorage&) override {}
        void OnStartGame(Fling::EntityStorage&) override {}
        void OnStopGame(Fling::EntityStorage&) override {}
        void Shutdown(Fling::EntityStorage&) override {}

        void Register(Fling::ModuleRegistration& t_Modules) override
        {
            Fling::RegisterEngine(t_Modules);
            t_Modules.Component<DumpCounter>([](Fling::TypeBuilder<DumpCounter>& t_Type)
                                             { t_Type.Field("ticks", &DumpCounter::Ticks); });
            t_Modules.Component<DumpScratch>([](Fling::TypeBuilder<DumpScratch>& t_Type)
                                             { t_Type.Field("value", &DumpScratch::Value); });
            t_Modules.System<DumpTickSystem>();
        }
    };

    void InitTestLogger()
    {
        Fling::Logger::Get().Init();
        Fling::Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Fling::Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    /** Make a world with two named entities, start play, and run t_Ticks ticks. */
    std::vector<Fling::World::StateDump> RunAndDump(const std::vector<uint64>& t_DumpTicks, const int t_Ticks)
    {
        using namespace Fling;
        World TestWorld(std::make_shared<DumpTestGame>());
        TestWorld.SetExecMode(ExecMode::Direct);
        TestWorld.SetSystemRunnerConfig(SystemRunnerConfig{.SimHz = TestSimHz});
        TestWorld.SetStateHashEnabled(true);
        TestWorld.SetStateDumpTicks(t_DumpTicks);
        TestWorld.Init();

        EntityStorage& Reg = TestWorld.GetStorage();
        for (const char* EntityName : {"Ball", "Wall"})
        {
            const Entity Each = Reg.Create();
            Reg.Add<NameComponent>(Each).Name = Name(EntityName);
            Reg.Add<Transform>(Each);
        }
        Reg.SetSingleton<DumpScratch>();

        TestWorld.RequestGameStart();
        std::vector<World::StateDump> Dumps;
        for (int i = 0; i < t_Ticks; ++i)
        {
            TestWorld.Update(OneTickFrame);
            for (World::StateDump& Dump : TestWorld.TakeStateDumps())
            {
                Dumps.push_back(std::move(Dump));
            }
        }
        TestWorld.Shutdown();
        return Dumps;
    }
} // namespace

TEST_CASE("World state dump at chosen ticks", "[world][state-dump]")
{
    using namespace Fling;
    InitTestLogger();

    const std::vector<World::StateDump> Dumps = RunAndDump({2, 4}, 5);
    REQUIRE(Dumps.size() == 2);
    REQUIRE(Dumps[0].Tick == 2);
    REQUIRE(Dumps[1].Tick == 4);

    const Json& Doc = Dumps[1].Doc;
    REQUIRE(Doc.GetUInt64("tick") == 4);
    REQUIRE(Doc.GetUInt64("simHz") == TestSimHz);
    REQUIRE(Doc.GetUInt64("entityCount") == 2);
    // On: the hash of the tick, as 16 hex digits
    REQUIRE(Doc.GetString("stateHash").size() == 16);
    REQUIRE(Doc.GetString("stateHash") != "0000000000000000");

    SECTION("Entities have their name, handle, id and components with every field")
    {
        const Json Entities = Doc.Get("entities");
        REQUIRE(Entities.IsArray());
        REQUIRE(Entities.Size() == 2);
        std::vector<String> Names;
        for (std::size_t i = 0; i < Entities.Size(); ++i)
        {
            const Json Entity = Entities.At(i);
            Names.push_back(Entity.GetString("name"));
            REQUIRE_FALSE(Entity.GetString("entity").empty());
            REQUIRE(Entity.GetUInt64("guid") != 0);
            // Four ticks moved it four units along x
            REQUIRE(Entity.Get("Transform").GetVec3("position").x == Catch::Approx(4.0f));
        }
        // In handle order, which is the order they were made in
        REQUIRE(Names == std::vector<String>{"Ball", "Wall"});
    }

    SECTION("Singletons: rollback state is in (even Transient), Keep state is out")
    {
        const Json Singletons = Doc.Get("singletons");
        REQUIRE(Singletons.Contains("Test.DumpCounter"));
        REQUIRE(Singletons.Get("Test.DumpCounter").GetInt("ticks") == 4);
        REQUIRE_FALSE(Singletons.Contains("Test.DumpScratch"));
    }

    SECTION("The same run writes the same document")
    {
        const std::vector<World::StateDump> Again = RunAndDump({2, 4}, 5);
        REQUIRE(Again.size() == 2);
        REQUIRE(Again[1].Doc.Dump(2) == Doc.Dump(2));
        // A different tick has a different state, so a different document
        REQUIRE(Dumps[0].Doc.Dump(2) != Doc.Dump(2));
    }
}

TEST_CASE("World state dump: a tick that does not run makes no dump", "[world][state-dump]")
{
    InitTestLogger();
    REQUIRE(RunAndDump({50}, 3).empty());
    REQUIRE(RunAndDump({}, 3).empty());
}
