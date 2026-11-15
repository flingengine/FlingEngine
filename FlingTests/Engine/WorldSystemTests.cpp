#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "ExecMode.h"
#include "Game.h"
#include "GameLane.h"
#include "Logger.h"
#include "PlatformTime.h"
#include "Strings/String.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// Fixture systems that exist only for these tests (Plans/ecs-systems.md section 3.3).

using namespace Fling;

namespace
{
    /** What the systems saw. One singleton, so the test can read it from the registry. */
    struct Counters
    {
        static constexpr ComponentInfo Component{.Name = "Test.Counters",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Keep};
        int Ticks = 0;
        int EditTicks = 0;
        int PlayBegan = 0;
        int PlayEnded = 0;
        uint64 LastTick = 0;
    };

    struct FrameCount
    {
        static constexpr ComponentInfo Component{.Name = "Test.FrameCount",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
        int Frames = 0;
    };

    /** Counts play ticks. Owns Counters. */
    class PlayCounterSystem final : public System<PlayCounterSystem, Access<Read<SimTime>, Write<Counters>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.PlayCounter", .In = Order::Simulation};

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
        static constexpr SystemInfo Info{.Name = "Test.EditCounter", .In = Order::Input, .RunIn = WorldRole::Edit};

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Counters>().EditTicks++; }
    };

    /** A variable rate system that runs in every role. */
    class FrameCounterSystem final : public System<FrameCounterSystem, Access<Write<FrameCount>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Test.FrameCounter", .In = Order::Presentation, .RunIn = WorldRole::All};

        void OnCreate(Setup& setup) const { setup.AddSingleton<FrameCount>(); }

        void Update(Context& ctx) const { ctx.Singleton<FrameCount>().Frames++; }
    };

    /** Game state that a rollback restores. The game below never registers it. */
    struct Score
    {
        static constexpr ComponentInfo Component{.Name = "Test.Score",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int Points = 0;
    };

    class ScoreSystem final : public System<ScoreSystem, Access<Write<Score>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Score", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const { ctx.Singleton<Score>().Points++; }
    };

    /** A second writer of Counters in the same phase, with no order: a schedule error. */
    class ConflictingSystem final : public System<ConflictingSystem, Access<Write<Counters>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Conflicting", .In = Order::Simulation};

        void FixedUpdate(Context&) const {}
    };

    class SystemsGame : public Game
    {
    public:
        void Init(Fling::EntityStorage&) override {}
        void OnStartGame(Fling::EntityStorage&) override {}
        void OnStopGame(Fling::EntityStorage&) override {}
        void Shutdown(Fling::EntityStorage&) override {}

        void Register(ModuleRegistration& t_Modules) override
        {
            t_Modules.System<PlayCounterSystem>();
            t_Modules.System<EditCounterSystem>();
            t_Modules.System<FrameCounterSystem>();
            if (AddConflict)
            {
                t_Modules.System<ConflictingSystem>();
            }
            if (AddUnregisteredState)
            {
                t_Modules.System<ScoreSystem>();
            }
            if (AddNameClash)
            {
                // A former name that is the type's own saved name
                t_Modules.Component<FrameCount>([](TypeBuilder<FrameCount>& t_Type)
                                                { t_Type.FormerNames({"Test.FrameCount"}); });
            }
        }

        bool AddConflict = false;
        bool AddNameClash = false;
        bool AddUnregisteredState = false;
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

    const auto TestGameOwner = std::make_shared<SystemsGame>();
    World TestWorld(TestGameOwner);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();
    TestWorld.SetSystemRunnerConfig(SystemRunnerConfig{.OneTickPerFrame = true});
    TestWorld.SetExecMode(ExecMode::Direct);
    REQUIRE(TestWorld.Init());
    REQUIRE(TestWorld.GetSchedule() != nullptr);
    REQUIRE(TestWorld.GetScheduleErrors().empty());
    REQUIRE(TestWorld.GetSystems().Size() == 3);

    // OnCreate ran before the first update
    REQUIRE(Reg.TryGetSingleton<Counters>() != nullptr);

    // Not playing: the Edit role runs, the Play role does not
    TestWorld.Update(0.016f);
    REQUIRE(Reg.Singleton<Counters>().EditTicks == 1);
    REQUIRE(Reg.Singleton<Counters>().Ticks == 0);
    REQUIRE(Reg.Singleton<FrameCount>().Frames == 1);

    // Play: OnPlayBegin, then the Play role
    TestWorld.RequestGameStart();
    TestWorld.Update(0.016f);
    REQUIRE(Reg.Singleton<Counters>().PlayBegan == 1);
    REQUIRE(Reg.Singleton<Counters>().Ticks == 1);
    REQUIRE(Reg.Singleton<Counters>().EditTicks == 1);
    REQUIRE(Reg.Singleton<FrameCount>().Frames == 2);

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
}

TEST_CASE("A world with a bad schedule does not start", "[world][ecs]")
{
    QuietLogger();

    const auto TestGameOwner = std::make_shared<SystemsGame>();
    SystemsGame& TestGame = *TestGameOwner;
    TestGame.AddConflict = true;
    World TestWorld(TestGameOwner);
    Fling::EntityStorage& Reg = TestWorld.GetStorage();

    REQUIRE_FALSE(TestWorld.Init());
    REQUIRE_FALSE(TestWorld.IsReadyForPlay());
    REQUIRE(TestWorld.GetSchedule() == nullptr);
    REQUIRE(TestWorld.GetScheduleErrors().find("Test.Conflicting and Test.PlayCounter both write Test.Counters") !=
            String::npos);

    // No system ran OnCreate
    REQUIRE(Reg.TryGetSingleton<Counters>() == nullptr);

    TestWorld.Shutdown();
}

TEST_CASE("A world with component names that clash does not start", "[world][ecs]")
{
    QuietLogger();

    const auto TestGameOwner = std::make_shared<SystemsGame>();
    SystemsGame& TestGame = *TestGameOwner;
    TestGame.AddNameClash = true;
    World TestWorld(TestGameOwner);

    REQUIRE_FALSE(TestWorld.Init());
    REQUIRE_FALSE(TestWorld.IsReadyForPlay());
    REQUIRE(TestWorld.GetScheduleErrors().find("Component types: ") != String::npos);
    REQUIRE(TestWorld.GetScheduleErrors().find("lists its own name") != String::npos);

    TestWorld.Shutdown();
}

TEST_CASE("A world whose systems use restored state that is not registered does not start", "[world][ecs]")
{
    QuietLogger();

    // The state hash covers exactly what a rollback restores, so the hash must see every
    // Restore type: one that it cannot see stops the world, it is not left out of the hash
    const auto TestGameOwner = std::make_shared<SystemsGame>();
    TestGameOwner->AddUnregisteredState = true;
    World TestWorld(TestGameOwner);

    REQUIRE_FALSE(TestWorld.Init());
    REQUIRE_FALSE(TestWorld.IsReadyForPlay());
    REQUIRE(TestWorld.GetScheduleErrors().find("system Test.Score uses Test.Score, which a rollback restores") !=
            String::npos);

    TestWorld.Shutdown();
}

namespace
{
    /** A setting that systems read and no system writes. Rollback state, so the state hash covers it. */
    struct Setting
    {
        static constexpr ComponentInfo Component{.Name = "Test.Setting",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int Step = 2;
    };

    struct SettingSum
    {
        static constexpr ComponentInfo Component{.Name = "Test.SettingSum",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Transient = true,
                                                 .Rollback = RollbackClass::Restore};
        int Value = 0;
    };

    /** What the frame side saw. */
    struct SettingShown
    {
        static constexpr ComponentInfo Component{.Name = "Test.SettingShown",
                                                 .Kind = ComponentKind::Singleton,
                                                 .Domain = ComponentDomain::Presentation,
                                                 .Transient = true};
        int Step = 0;
        int Sum = 0;
    };

    class SettingStepSystem final : public System<SettingStepSystem, Access<Read<Setting>, Write<SettingSum>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.SettingStep", .In = Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            ctx.Singleton<SettingSum>().Value += ctx.Singleton<const Setting>().Step;
        }
    };

    class SettingShowSystem final
        : public System<SettingShowSystem, Access<Read<Setting>, Read<SettingSum>, Write<SettingShown>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.SettingShow", .In = Order::Presentation};

        void Update(Context& ctx) const
        {
            ctx.Singleton<SettingShown>().Step = ctx.Singleton<const Setting>().Step;
            ctx.Singleton<SettingShown>().Sum = ctx.Singleton<const SettingSum>().Value;
        }
    };

    class SettingGame final : public Game
    {
    public:
        explicit SettingGame(const std::optional<int> t_SetupStep) : m_SetupStep(t_SetupStep) {}

        void Init(EntityStorage& t_Storage) override
        {
            if (m_SetupStep)
            {
                t_Storage.SetSingleton<Setting>().Step = *m_SetupStep;
            }
        }

        void Register(ModuleRegistration& t_Modules) override
        {
            RegisterEngine(t_Modules);
            t_Modules.Component<Setting>([](TypeBuilder<Setting>& t_Type) { t_Type.Field("Step", &Setting::Step); });
            t_Modules.Component<SettingSum>([](TypeBuilder<SettingSum>& t_Type)
                                            { t_Type.Field("Value", &SettingSum::Value); });
            t_Modules.System<SettingStepSystem>();
            t_Modules.System<SettingShowSystem>();
        }

        void OnStartGame(EntityStorage&) override {}
        void OnStopGame(EntityStorage&) override {}
        void Shutdown(EntityStorage&) override {}

    private:
        std::optional<int> m_SetupStep;
    };

    struct SettingRun
    {
        std::vector<uint64> Hashes;
        SettingShown Shown;
    };

    /** Plays 10 ticks, one frame after each, on a virtual clock. */
    SettingRun PlaySetting(const ExecMode t_Mode, const std::optional<int> t_SetupStep)
    {
        QuietLogger();
        PlatformTime::ScopedVirtualClock Clock(0);
        World TestWorld(std::make_shared<SettingGame>(t_SetupStep));
        TestWorld.SetExecMode(t_Mode);
        TestWorld.SetTickClock([]() { return PlatformTime::NowNanoseconds(); });
        TestWorld.SetTickTimelineStart(0);
        GameLaneDesc Desc;
        Desc.Pacing = GameLanePacing::Stepped;
        TestWorld.SetGameLaneDesc(Desc);
        TestWorld.KeepTickRecords();
        REQUIRE(TestWorld.Init());
        REQUIRE(TestWorld.GetSchedule()->Dump().find(
                    "  Test.Setting (singleton), read by Test.SettingShow, Test.SettingStep") != String::npos);

        TestWorld.RequestGameStart();
        TestWorld.Update(0.001f);
        (void)TestWorld.TakeNewTickRecords();
        const int64 Step = TestWorld.GetSystemRunnerConfig().FixedStep().count();
        for (int64 Frame = 1; Frame <= 10; ++Frame)
        {
            Clock.Clock().Set(Frame * Step);
            if (TestWorld.GetGameLane() != nullptr)
            {
                TestWorld.StepGameLane(1);
            }
            TestWorld.Update(0.001f);
        }
        SettingRun Run;
        for (const TickRecord& Record : TestWorld.TakeNewTickRecords())
        {
            Run.Hashes.push_back(Record.Stamp.StateHash);
        }
        Run.Shown = TestWorld.GetPresentedStorage().Singleton<SettingShown>();
        TestWorld.RequestGameStop();
        TestWorld.Update(0.001f);
        TestWorld.Shutdown();
        return Run;
    }
} // namespace

TEST_CASE("A singleton that no system writes has its default or setup value in every exec mode", "[world][ecs]")
{
    const SettingRun Direct = PlaySetting(ExecMode::Direct, std::nullopt);
    REQUIRE(Direct.Hashes.size() == 10);
    REQUIRE(Direct.Shown.Step == 2);

    // The value the game sets at setup is the one the systems read, and the state hash sees it
    const SettingRun SetUp = PlaySetting(ExecMode::Direct, 5);
    REQUIRE(SetUp.Shown.Step == 5);
    REQUIRE(SetUp.Shown.Sum * 2 == Direct.Shown.Sum * 5);
    REQUIRE(SetUp.Hashes != Direct.Hashes);

    for (const ExecMode Mode : {ExecMode::Inline, ExecMode::Threaded})
    {
        CAPTURE(ExecModeName(Mode));
        const SettingRun Run = PlaySetting(Mode, std::nullopt);
        CHECK(Run.Hashes == Direct.Hashes);
        CHECK(Run.Shown.Step == Direct.Shown.Step);
        CHECK(Run.Shown.Sum == Direct.Shown.Sum);
        CHECK(PlaySetting(Mode, 5).Hashes == SetUp.Hashes);
    }
}
