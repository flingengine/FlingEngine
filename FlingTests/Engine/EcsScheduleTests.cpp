#include "Ecs/Schedule.h"
#include "Ecs/SystemRegistry.h"
#include "FlingTypes.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <deque>
#include <string>
#include <vector>

using namespace Fling;

namespace
{
    /** Fake system descriptions. Holds the access rows so the spans stay valid. */
    class FakeSystems
    {
    public:
        struct Use
        {
            const char* Type;
            AccessMode Mode;
        };

        FakeSystems& Add(SystemInfo info, std::vector<Use> uses)
        {
            std::vector<AccessEntry>& rows = m_Rows.emplace_back();
            for (const Use& use : uses)
            {
                rows.push_back(AccessEntry{HashName(use.Type), use.Mode, use.Type});
            }
            m_Descs.push_back(SystemDesc{info, rows});
            return *this;
        }

        ScheduleResult Build() const
        {
            std::vector<const SystemDesc*> pointers;
            for (const SystemDesc& desc : m_Descs)
            {
                pointers.push_back(&desc);
            }
            return BuildSchedule(pointers);
        }

    private:
        std::deque<std::vector<AccessEntry>> m_Rows;
        std::deque<SystemDesc> m_Descs;
    };

    constexpr AccessMode R = AccessMode::Read;
    constexpr AccessMode W = AccessMode::Write;
    constexpr AccessMode A = AccessMode::Append;
    constexpr AccessMode C = AccessMode::Consume;

    SystemInfo Sim(const char* name)
    {
        return SystemInfo{.Name = name, .Phase = Phase::Simulation};
    }

    /** The names in each stage of one phase. */
    std::vector<std::vector<std::string>> StageNames(const Schedule& schedule, const Phase phase)
    {
        std::vector<std::vector<std::string>> stages;
        for (const ScheduleStage& stage : schedule.GetPhase(phase).Stages)
        {
            std::vector<std::string>& names = stages.emplace_back();
            for (const uint32 i : stage.Systems)
            {
                names.emplace_back(schedule.Systems[i]->Info.Name);
            }
        }
        return stages;
    }

    bool HasError(const ScheduleResult& result, const ScheduleErrorKind kind)
    {
        return std::any_of(
            result.Errors.begin(), result.Errors.end(), [kind](const ScheduleError& e) { return e.Kind == kind; });
    }

    using Stages = std::vector<std::vector<std::string>>;
} // namespace

TEST_CASE("A writer runs before a reader", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.Reader"), {{"X", R}});
    systems.Add(Sim("Test.Writer"), {{"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.Writer"}, {"Test.Reader"}});
    REQUIRE(result.Value.Edges.size() == 1);
    REQUIRE(result.Value.Edges[0].Reason == EdgeReason::WriteRead);
}

TEST_CASE("Readers of the same type share a stage", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.B"), {{"X", R}});
    systems.Add(Sim("Test.A"), {{"X", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.A", "Test.B"}});
    REQUIRE(result.Value.Edges.empty());
}

TEST_CASE("Every appender runs before the consumer", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.Resolve"), {{"Q", C}});
    systems.Add(Sim("Test.Gun"), {{"Q", A}});
    systems.Add(Sim("Test.Ability"), {{"Q", A}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.Ability", "Test.Gun"}, {"Test.Resolve"}});
}

TEST_CASE("Two writers with no order are an error", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.Rotator"), {{"Transform", W}});
    systems.Add(Sim("Test.Mover"), {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE_FALSE(result.Ok());
    REQUIRE(result.Errors.size() == 1);
    REQUIRE(result.Errors[0].Kind == ScheduleErrorKind::WriteConflict);

    const std::string& message = result.Errors[0].Message;
    REQUIRE(message.find("Test.Mover and Test.Rotator both write Transform") != std::string::npos);
    REQUIRE(message.find(".After = \"Test.Mover\" to Test.Rotator") != std::string::npos);
}

TEST_CASE("Two writers in worlds that never meet are not an error", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.EditGizmo", .Phase = Phase::Simulation, .RunIn = WorldRole::Edit},
                {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Mover", .Phase = Phase::Simulation, .RunIn = WorldRole::Play},
                {{"Transform", W}});

    REQUIRE(systems.Build().Ok());
}

TEST_CASE("After and Before order two writers", "[ecs][schedule]")
{
    SECTION("After")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phase::Simulation, .After = "Test.B"}, {{"X", W}});
        systems.Add(Sim("Test.B"), {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.B"}, {"Test.A"}});
        REQUIRE(result.Value.Edges[0].Reason == EdgeReason::After);
    }

    SECTION("Before")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.B", .Phase = Phase::Simulation, .Before = "Test.A"}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.B"}, {"Test.A"}});
    }

    SECTION("An explicit order replaces the inferred edge")
    {
        // The writer would run first, but .After puts the reader first.
        FakeSystems systems;
        systems.Add(Sim("Test.Reader"), {{"X", R}});
        systems.Add(SystemInfo{.Name = "Test.Writer", .Phase = Phase::Simulation, .After = "Test.Reader"}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.Reader"}, {"Test.Writer"}});
    }

    SECTION("A list of names")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Last", .Phase = Phase::Simulation, .After = {"Test.A", "Test.B"}}, {});
        systems.Add(Sim("Test.A"), {});
        systems.Add(Sim("Test.B"), {});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.A", "Test.B"}, {"Test.Last"}});
    }
}

TEST_CASE("A cycle is reported with its path", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.A"), {{"X", W}, {"Y", R}});
    systems.Add(Sim("Test.B"), {{"Y", W}, {"Z", R}});
    systems.Add(Sim("Test.C"), {{"Z", W}, {"X", R}});
    systems.Add(Sim("Test.Free"), {{"X", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE_FALSE(result.Ok());
    REQUIRE(result.Errors.size() == 1);

    const ScheduleError& error = result.Errors[0];
    REQUIRE(error.Kind == ScheduleErrorKind::Cycle);
    REQUIRE(error.Systems.size() == 3);
    REQUIRE(error.Message.find("Test.A -> Test.C -> Test.B -> Test.A") != std::string::npos);
    REQUIRE(error.Message.find("Test.A before Test.C (writes X)") != std::string::npos);
    REQUIRE(error.Message.find("Test.B before Test.A (writes Y)") != std::string::npos);
}

TEST_CASE("Ties are broken by name, whatever the add order", "[ecs][schedule]")
{
    const std::vector<std::string> names{"Test.D", "Test.A", "Test.C", "Test.B"};
    std::vector<std::string> order = names;
    std::sort(order.begin(), order.end());

    Stages first;
    do
    {
        FakeSystems systems;
        for (const std::string& name : order)
        {
            // The descriptions keep a pointer to the name, so point into names, not order.
            const char* stored = std::find(names.begin(), names.end(), name)->c_str();
            systems.Add(Sim(stored), {{"X", name == "Test.C" ? W : R}});
        }

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        const Stages stages = StageNames(result.Value, Phase::Simulation);
        if (first.empty())
        {
            first = stages;
            REQUIRE(first == Stages{{"Test.C"}, {"Test.A", "Test.B", "Test.D"}});
        }
        REQUIRE(stages == first);
    } while (std::next_permutation(order.begin(), order.end()));
}

TEST_CASE("Bad names are errors", "[ecs][schedule]")
{
    SECTION("Unknown After name")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phase::Simulation, .After = "Test.Missing"}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::UnknownOrderName));
    }

    SECTION("After names a system in another phase")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phase::Simulation, .After = "Test.Input"}, {});
        systems.Add(SystemInfo{.Name = "Test.Input", .Phase = Phase::Input}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::OrderAcrossPhases));
    }

    SECTION("Duplicate name")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {});
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phase::Extract}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::DuplicateName));
    }

    SECTION("No name")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Phase = Phase::Simulation}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::MissingName));
    }
}

TEST_CASE("A queue has one consumer", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.ResolveA", .Phase = Phase::ResolveDeferred}, {{"Q", C}});
    systems.Add(SystemInfo{.Name = "Test.ResolveB", .Phase = Phase::ResolveDeferred}, {{"Q", C}});
    REQUIRE(HasError(systems.Build(), ScheduleErrorKind::ConsumeConflict));
}

TEST_CASE("A component written at both update rates is an error", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.Mover"), {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Smooth", .Phase = Phase::PostSimulation}, {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(HasError(result, ScheduleErrorKind::FixedAndFrameWrite));
    REQUIRE(result.ErrorReport().find("Transform is written by Test.Mover") != std::string::npos);
}

TEST_CASE("Phases keep their own stages", "[ecs][schedule]")
{
    // A writer in a later phase does not make an edge to a reader in an earlier one.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Extract", .Phase = Phase::Extract}, {{"X", R}});
    systems.Add(SystemInfo{.Name = "Test.Input", .Phase = Phase::Input}, {{"Input", W}});
    systems.Add(Sim("Test.Move"), {{"Input", R}, {"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(result.Value.Edges.empty());

    std::vector<std::string> run;
    for (const uint32 i : result.Value.RunOrder())
    {
        run.emplace_back(result.Value.Systems[i]->Info.Name);
    }
    REQUIRE(run == std::vector<std::string>{"Test.Input", "Test.Move", "Test.Extract"});
}

TEST_CASE("Dump lists phases, stages, writes, and reasons", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Engine.Input", .Phase = Phase::Input}, {{"InputState", W}});
    systems.Add(Sim("Sandbox.Mover"), {{"Mover", W}, {"Transform", W}});
    systems.Add(SystemInfo{.Name = "Sandbox.Rotator", .Phase = Phase::Simulation, .After = "Sandbox.Mover"},
                {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());

    const std::string dump = result.Value.Dump();
    REQUIRE(dump.find("Phase Input\n") != std::string::npos);
    REQUIRE(dump.find("Phase Simulation\n") != std::string::npos);
    REQUIRE(dump.find("Phase Extract") == std::string::npos);
    REQUIRE(dump.find("writes Mover, Transform") != std::string::npos);
    REQUIRE(dump.find("stage 1  Sandbox.Rotator") != std::string::npos);
    REQUIRE(dump.find("after Sandbox.Mover: .After") != std::string::npos);
}

namespace
{
    struct Score
    {
        static constexpr ComponentInfo Component{.Name = "Test.Score"};
        int Value = 0;
    };

    class ScoreWriter final : public System<ScoreWriter, Access<Write<Score>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.ScoreWriter", .Phase = Phase::Simulation};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class ScoreReader final : public System<ScoreReader, Access<Read<Score>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.ScoreReader", .Phase = Phase::Simulation};

        void FixedUpdate(Context& /*ctx*/) const {}
    };
} // namespace

TEST_CASE("SystemRegistry builds a schedule from its systems", "[ecs][schedule]")
{
    SystemRegistry registry;
    registry.Add<ScoreReader>();
    registry.Add<ScoreWriter>();
    REQUIRE(registry.Size() == 2);
    REQUIRE(registry.Descs()[0] == &registry.At(0).Describe());

    const ScheduleResult result = registry.BuildSchedule();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value, Phase::Simulation) == Stages{{"Test.ScoreWriter"}, {"Test.ScoreReader"}});
}
