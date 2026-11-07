#include "Ecs/Schedule.h"
#include "Ecs/SystemRegistry.h"
#include "FlingTypes.h"
#include "Strings/String.h"

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

        /** Adds a phase with no systems in it yet. The phase must outlive the build. */
        FakeSystems& AddPhase(const PhaseInfo& phase)
        {
            m_Phases.push_back(&phase);
            return *this;
        }

        FakeSystems& Order(const char* first, const char* second)
        {
            m_Orders.push_back(ScheduleOrder{first, second});
            return *this;
        }

        ScheduleResult Build() const
        {
            std::vector<const SystemDesc*> pointers;
            for (const SystemDesc& desc : m_Descs)
            {
                pointers.push_back(&desc);
            }
            return BuildSchedule(ScheduleInput{pointers, m_Phases, m_Orders});
        }

    private:
        std::deque<std::vector<AccessEntry>> m_Rows;
        std::deque<SystemDesc> m_Descs;
        std::vector<const PhaseInfo*> m_Phases;
        std::vector<ScheduleOrder> m_Orders;
    };

    constexpr AccessMode R = AccessMode::Read;
    constexpr AccessMode W = AccessMode::Write;
    constexpr AccessMode A = AccessMode::Append;
    constexpr AccessMode C = AccessMode::Consume;

    SystemInfo Sim(const char* name)
    {
        return SystemInfo{.Name = name, .Phase = Phases::Simulation};
    }

    /** The names in each stage of one update rate, for every segment in order. */
    std::vector<std::vector<String>> StageNames(const Schedule& schedule, const UpdateRate rate = UpdateRate::Fixed)
    {
        std::vector<std::vector<String>> stages;
        for (const ScheduleSegment& segment : schedule.GetSegments(rate))
        {
            for (const ScheduleStage& stage : segment.Stages)
            {
                std::vector<String>& names = stages.emplace_back();
                for (const uint32 i : stage.Systems)
                {
                    names.emplace_back(schedule.Systems[i]->Info.Name);
                }
            }
        }
        return stages;
    }

    /** The system names of one update rate in run order. */
    std::vector<String> RunNames(const Schedule& schedule, const UpdateRate rate = UpdateRate::Fixed)
    {
        std::vector<String> names;
        for (const uint32 i : schedule.RunOrder(rate))
        {
            names.emplace_back(schedule.Systems[i]->Info.Name);
        }
        return names;
    }

    bool HasText(const ScheduleResult& result, const StringView text)
    {
        return result.ErrorReport().find(text) != String::npos;
    }

    bool HasError(const ScheduleResult& result, const ScheduleErrorKind kind)
    {
        return std::any_of(
            result.Errors.begin(), result.Errors.end(), [kind](const ScheduleError& e) { return e.Kind == kind; });
    }

    using Stages = std::vector<std::vector<String>>;
} // namespace

TEST_CASE("A writer runs before a reader", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.Reader"), {{"X", R}});
    systems.Add(Sim("Test.Writer"), {{"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value) == Stages{{"Test.Writer"}, {"Test.Reader"}});
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
    REQUIRE(StageNames(result.Value) == Stages{{"Test.A", "Test.B"}});
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
    REQUIRE(StageNames(result.Value) == Stages{{"Test.Ability", "Test.Gun"}, {"Test.Resolve"}});
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

    const String& message = result.Errors[0].Message;
    REQUIRE(message.find("Test.Mover and Test.Rotator both write Transform") != String::npos);
    REQUIRE(message.find(".After = \"Test.Mover\" to Test.Rotator") != String::npos);
}

TEST_CASE("An order through a third system orders two writers", "[ecs][schedule]")
{
    // Spawn before Rotator, and Mover after Rotator: Spawn and Mover are ordered too
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Spawn", .Phase = Phases::Simulation, .Before = "Test.Rotator"},
                {{"Transform", W}});
    systems.Add(Sim("Test.Rotator"), {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Mover", .Phase = Phases::Simulation, .After = "Test.Rotator"},
                {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Spawn", "Test.Rotator", "Test.Mover"});
}

TEST_CASE("Two writers in worlds that never meet are not an error", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.EditGizmo", .Phase = Phases::Simulation, .RunIn = WorldRole::Edit},
                {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Mover", .Phase = Phases::Simulation, .RunIn = WorldRole::Play},
                {{"Transform", W}});

    REQUIRE(systems.Build().Ok());
}

TEST_CASE("After and Before order two writers", "[ecs][schedule]")
{
    SECTION("After")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phases::Simulation, .After = "Test.B"}, {{"X", W}});
        systems.Add(Sim("Test.B"), {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.B"}, {"Test.A"}});
        REQUIRE(result.Value.Edges[0].Reason == EdgeReason::After);
    }

    SECTION("Before")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.B", .Phase = Phases::Simulation, .Before = "Test.A"}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.B"}, {"Test.A"}});
    }

    SECTION("An explicit order replaces the inferred edge")
    {
        // The writer would run first, but .After puts the reader first.
        FakeSystems systems;
        systems.Add(Sim("Test.Reader"), {{"X", R}});
        systems.Add(SystemInfo{.Name = "Test.Writer", .Phase = Phases::Simulation, .After = "Test.Reader"}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.Reader"}, {"Test.Writer"}});
    }

    SECTION("A list of names")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Last", .Phase = Phases::Simulation, .After = {"Test.A", "Test.B"}}, {});
        systems.Add(Sim("Test.A"), {});
        systems.Add(Sim("Test.B"), {});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.A", "Test.B"}, {"Test.Last"}});
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
    REQUIRE(error.Message.find("Test.A -> Test.C -> Test.B -> Test.A") != String::npos);
    REQUIRE(error.Message.find("Test.A before Test.C (writes X)") != String::npos);
    REQUIRE(error.Message.find("Test.B before Test.A (writes Y)") != String::npos);
}

TEST_CASE("Ties are broken by name, whatever the add order", "[ecs][schedule]")
{
    const std::vector<String> names{"Test.D", "Test.A", "Test.C", "Test.B"};
    std::vector<String> order = names;
    std::sort(order.begin(), order.end());

    Stages first;
    do
    {
        FakeSystems systems;
        for (const StringView name : order)
        {
            // The descriptions keep a pointer to the name, so point into names, not order.
            const char* stored = std::find(names.begin(), names.end(), name)->c_str();
            systems.Add(Sim(stored), {{"X", name == "Test.C" ? W : R}});
        }

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        const Stages stages = StageNames(result.Value);
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
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phases::Simulation, .After = "Test.Missing"}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::UnknownOrderName));
    }

    SECTION("After names a system of the other update rate")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phases::Simulation, .After = "Test.Draw"}, {});
        systems.Add(SystemInfo{.Name = "Test.Draw", .Phase = Phases::Extract}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::OrderAcrossUpdateRates));
    }

    SECTION("No phase")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A"}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::MissingPhase));
    }

    SECTION("Duplicate name")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {});
        systems.Add(SystemInfo{.Name = "Test.A", .Phase = Phases::Extract}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::DuplicateName));
    }

    SECTION("No name")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Phase = Phases::Simulation}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::MissingName));
    }
}

TEST_CASE("A queue has one consumer", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.ResolveA", .Phase = Phases::ResolveDeferred}, {{"Q", C}});
    systems.Add(SystemInfo{.Name = "Test.ResolveB", .Phase = Phases::ResolveDeferred}, {{"Q", C}});
    REQUIRE(HasError(systems.Build(), ScheduleErrorKind::ConsumeConflict));
}

TEST_CASE("A component written at both update rates is an error", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.Mover"), {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Smooth", .Phase = Phases::PostSimulation}, {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(HasError(result, ScheduleErrorKind::FixedAndFrameWrite));
    REQUIRE(result.ErrorReport().find("Transform is written by Test.Mover") != String::npos);
}

TEST_CASE("Phases keep their own stages", "[ecs][schedule]")
{
    // A writer in a later phase does not make an edge to a reader in an earlier one.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Extract", .Phase = Phases::Extract}, {{"X", R}});
    systems.Add(SystemInfo{.Name = "Test.Input", .Phase = Phases::Input}, {{"Input", W}});
    systems.Add(Sim("Test.Move"), {{"Input", R}, {"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(result.Value.Edges.empty());

    std::vector<String> run;
    for (const uint32 i : result.Value.RunOrder())
    {
        run.emplace_back(result.Value.Systems[i]->Info.Name);
    }
    REQUIRE(run == std::vector<String>{"Test.Input", "Test.Move", "Test.Extract"});
}

TEST_CASE("Dump lists phases, stages, writes, and reasons", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Engine.Input", .Phase = Phases::Input}, {{"InputState", W}});
    systems.Add(Sim("Sandbox.Mover"), {{"Mover", W}, {"Transform", W}});
    systems.Add(SystemInfo{.Name = "Sandbox.Rotator", .Phase = Phases::Simulation, .After = "Sandbox.Mover"},
                {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());

    const String dump = result.Value.Dump();
    REQUIRE(dump.find("Fixed (once per tick)\n") != String::npos);
    REQUIRE(dump.find("phases: Engine.Input, Engine.Simulation, Engine.ResolveDeferred, Engine.Transform\n") !=
            String::npos);
    REQUIRE(dump.find("Frame (once per frame)") == String::npos);
    REQUIRE(dump.find("[Engine.Simulation]") != String::npos);
    REQUIRE(dump.find("writes Mover, Transform") != String::npos);
    REQUIRE(dump.find("stage 2  Sandbox.Rotator") != String::npos);
    REQUIRE(dump.find("after Sandbox.Mover: .After") != String::npos);
    REQUIRE(dump.find("flush: end of tick") != String::npos);
}

// ---- Open phases (Plans/ecs-systems.md 3.8) ------------------------------------------------

namespace
{
    // Two modules that do not know each other. Both run after Simulation and before ResolveDeferred.
    inline constexpr PhaseInfo ModAPhase{.Name = "ModA.Abilities",
                                         .UpdateRate = UpdateRate::Fixed,
                                         .After = "Engine.Simulation",
                                         .Before = "Engine.ResolveDeferred"};
    inline constexpr PhaseInfo ModBPhase{.Name = "ModB.Combo",
                                         .UpdateRate = UpdateRate::Fixed,
                                         .After = "Engine.Simulation",
                                         .Before = "Engine.ResolveDeferred"};

    inline constexpr PhaseInfo LatePhase{
        .Name = "Game.Late", .UpdateRate = UpdateRate::Fixed, .After = "Engine.ResolveDeferred"};
    inline constexpr PhaseInfo FlushPhase{.Name = "Game.Flush",
                                          .UpdateRate = UpdateRate::Fixed,
                                          .After = "Engine.Simulation",
                                          .Before = "Engine.ResolveDeferred",
                                          .FlushAfter = true};
    inline constexpr PhaseInfo LooseFlushPhase{
        .Name = "Game.LooseFlush", .UpdateRate = UpdateRate::Fixed, .After = "Engine.Input", .FlushAfter = true};
    inline constexpr PhaseInfo UnknownAnchorPhase{
        .Name = "Game.Lost", .UpdateRate = UpdateRate::Fixed, .After = "Engine.Simulaton"};
    inline constexpr PhaseInfo WrongRatePhase{
        .Name = "Game.WrongRate", .UpdateRate = UpdateRate::Fixed, .After = "Engine.Extract"};
    inline constexpr PhaseInfo LoopA{.Name = "Game.LoopA", .UpdateRate = UpdateRate::Fixed, .After = "Game.LoopB"};
    inline constexpr PhaseInfo LoopB{.Name = "Game.LoopB", .UpdateRate = UpdateRate::Fixed, .After = "Game.LoopA"};
    inline constexpr PhaseInfo FramePhaseWithFlush{
        .Name = "Game.FrameFlush", .UpdateRate = UpdateRate::Frame, .FlushAfter = true};
    inline constexpr PhaseInfo SameNameAsModA{.Name = "ModA.Abilities", .UpdateRate = UpdateRate::Frame};
} // namespace

TEST_CASE("Two modules' phases are ordered by the data they share", "[ecs][schedule][phases]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "ModB.ComboSystem", .Phase = ModBPhase}, {{"Cooldown", R}});
    systems.Add(SystemInfo{.Name = "ModA.CooldownSystem", .Phase = ModAPhase}, {{"Cooldown", W}});
    systems.Add(Sim("Engine.Gameplay"), {{"Cooldown", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");

    // Gameplay is in Simulation, which both phases follow. ModA writes what ModB reads.
    REQUIRE(RunNames(result.Value) ==
            std::vector<String>{"Engine.Gameplay", "ModA.CooldownSystem", "ModB.ComboSystem"});
    REQUIRE(result.Value.Edges.size() == 1);
    REQUIRE(result.Value.Edges[0].Reason == EdgeReason::WriteRead);
}

TEST_CASE("Unrelated phases with no shared data share a stage", "[ecs][schedule][phases]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "ModA.Thing", .Phase = ModAPhase}, {{"X", W}});
    systems.Add(SystemInfo{.Name = "ModB.Other", .Phase = ModBPhase}, {{"Y", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value) == Stages{{"ModA.Thing", "ModB.Other"}});
}

TEST_CASE("Two unordered phases that write one type are an error", "[ecs][schedule][phases]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "ModA.Mover", .Phase = ModAPhase}, {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "ModB.Mover", .Phase = ModBPhase}, {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(HasError(result, ScheduleErrorKind::WriteConflict));
    REQUIRE(HasText(result, "ModA.Mover and ModB.Mover both write Transform"));
    REQUIRE(HasText(result, "phase ModA.Abilities and phase ModB.Combo"));
}

TEST_CASE("Order from outside fixes a conflict between two modules", "[ecs][schedule][phases]")
{
    SECTION("Order two phases")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "ModA.Mover", .Phase = ModAPhase}, {{"Transform", W}});
        systems.Add(SystemInfo{.Name = "ModB.Mover", .Phase = ModBPhase}, {{"Transform", W}});
        systems.Order("ModB.Combo", "ModA.Abilities");

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"ModB.Mover", "ModA.Mover"});
    }

    SECTION("Order two systems")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "ModA.Mover", .Phase = ModAPhase}, {{"Transform", W}});
        systems.Add(SystemInfo{.Name = "ModB.Mover", .Phase = ModBPhase}, {{"Transform", W}});
        systems.Order("ModB.Mover", "ModA.Mover");

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"ModB.Mover", "ModA.Mover"});
        REQUIRE(result.Value.Edges[0].Reason == EdgeReason::Order);
    }

    SECTION("An Order name that does not exist")
    {
        FakeSystems systems;
        systems.Order("ModA.Abilities", "Nobody");
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::UnknownOrderName));
    }
}

TEST_CASE("Phase order wins over a data edge that points back", "[ecs][schedule][phases]")
{
    // Late writes X after ResolveDeferred. Gameplay reads X in Simulation, which is earlier,
    // so it sees the value from the previous tick. That is not an edge and not a cycle.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Game.LateWriter", .Phase = LatePhase}, {{"X", W}});
    systems.Add(Sim("Engine.Reader"), {{"X", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(result.Value.Edges.empty());
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Engine.Reader", "Game.LateWriter"});
}

TEST_CASE("A system After can cross phases of one update rate", "[ecs][schedule][phases]")
{
    SECTION("It agrees with phase order")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Game.Late", .Phase = LatePhase, .After = "Engine.Gameplay"}, {});
        systems.Add(Sim("Engine.Gameplay"), {});
        REQUIRE(systems.Build().Ok());
    }

    SECTION("It contradicts phase order, which is a cycle")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Engine.Gameplay", .Phase = Phases::Simulation, .After = "Game.Late"}, {});
        systems.Add(SystemInfo{.Name = "Game.Late", .Phase = LatePhase}, {});

        const ScheduleResult result = systems.Build();
        REQUIRE(HasError(result, ScheduleErrorKind::Cycle));
        REQUIRE(HasText(result, "Game.Late before Engine.Gameplay (.After)"));
    }
}

TEST_CASE("Phase declaration errors", "[ecs][schedule][phases]")
{
    SECTION("An anchor name that does not exist")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Game.A", .Phase = UnknownAnchorPhase}, {});
        const ScheduleResult result = systems.Build();
        REQUIRE(HasError(result, ScheduleErrorKind::UnknownPhase));
        REQUIRE(HasText(result, "\"Engine.Simulaton\""));
    }

    SECTION("A fixed phase placed against a frame phase")
    {
        FakeSystems systems;
        systems.AddPhase(WrongRatePhase);
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::PhaseAcrossUpdateRates));
    }

    SECTION("Phases in a loop")
    {
        FakeSystems systems;
        systems.AddPhase(LoopA).AddPhase(LoopB);
        const ScheduleResult result = systems.Build();
        REQUIRE(HasError(result, ScheduleErrorKind::Cycle));
        REQUIRE(HasText(result, "Game.LoopA (end) -> Game.LoopB (begin)"));
    }

    SECTION("Two different phases with one name")
    {
        FakeSystems systems;
        systems.AddPhase(ModAPhase).AddPhase(SameNameAsModA);
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::DuplicatePhase));
    }

    SECTION("FlushAfter on a frame phase")
    {
        FakeSystems systems;
        systems.AddPhase(FramePhaseWithFlush);
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::FlushInFramePhase));
    }
}

TEST_CASE("A FlushAfter phase splits the tick into segments", "[ecs][schedule][phases]")
{
    FakeSystems systems;
    systems.Add(Sim("Engine.Gameplay"), {});
    systems.Add(SystemInfo{.Name = "Game.Flushing", .Phase = FlushPhase}, {});
    systems.Add(SystemInfo{.Name = "Engine.Resolve", .Phase = Phases::ResolveDeferred}, {});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");

    const std::vector<ScheduleSegment>& segments = result.Value.GetSegments(UpdateRate::Fixed);
    REQUIRE(segments.size() == 2);
    REQUIRE(segments[0].FlushPhase == &FlushPhase);
    REQUIRE(segments[1].FlushPhase == nullptr);
    REQUIRE(StageNames(result.Value) == Stages{{"Engine.Gameplay"}, {"Game.Flushing"}, {"Engine.Resolve"}});
    REQUIRE(result.Value.Dump().find("flush: end of phase Game.Flush") != String::npos);
}

TEST_CASE("A FlushAfter phase must be ordered against every phase with systems", "[ecs][schedule][phases]")
{
    // LooseFlush is only after Input, so nothing orders it against ModA's phase.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Game.Flushing", .Phase = LooseFlushPhase}, {});
    systems.Add(SystemInfo{.Name = "ModA.Thing", .Phase = ModAPhase}, {});

    const ScheduleResult result = systems.Build();
    REQUIRE(HasError(result, ScheduleErrorKind::FlushNotOrdered));
    REQUIRE(HasText(result, "Game.LooseFlush has .FlushAfter, but nothing orders it against phase ModA.Abilities"));
}

TEST_CASE("Phases are listed in run order", "[ecs][schedule][phases]")
{
    FakeSystems systems;
    systems.AddPhase(LatePhase).AddPhase(ModAPhase);

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());

    std::vector<String> names;
    for (const PhaseInfo* phase : result.Value.Phases)
    {
        names.emplace_back(phase->Name);
    }
    REQUIRE(names == std::vector<String>{"Engine.Input",
                                         "Engine.Simulation",
                                         "ModA.Abilities",
                                         "Engine.ResolveDeferred",
                                         "Engine.Transform",
                                         "Game.Late",
                                         "Engine.PostSimulation",
                                         "Engine.Extract",
                                         "Engine.Editor"});
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
        static constexpr SystemInfo Info{.Name = "Test.ScoreWriter", .Phase = Phases::Simulation};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class ScoreReader final : public System<ScoreReader, Access<Read<Score>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.ScoreReader", .Phase = Phases::Simulation};

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
    REQUIRE(StageNames(result.Value) == Stages{{"Test.ScoreWriter"}, {"Test.ScoreReader"}});
}
