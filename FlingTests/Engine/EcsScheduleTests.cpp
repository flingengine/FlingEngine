#include "Components/Transform.h"
#include "Ecs/EngineOrder.h"
#include "Ecs/Order.h"
#include "Ecs/Schedule.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "FlingTypes.h"
#include "Strings/String.h"
#include "Systems/TransformSystem.h"
#include "TestSystems/TestCamera.h"
#include "TypeName.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <deque>
#include <string>
#include <vector>

using namespace Fling;

namespace
{
    constexpr ComponentKind Table = ComponentKind::Table;
    constexpr ComponentKind Single = ComponentKind::Singleton;
    constexpr ComponentKind Queue = ComponentKind::Queue;
    constexpr ComponentDomain Pres = ComponentDomain::Presentation;
    constexpr ComponentDomain SimDomain = ComponentDomain::Simulation;
    constexpr RollbackClass Rebuild = RollbackClass::Rebuild;

    /**
     * The rest of a fake description. Outside FakeSystems because Clang does not allow a
     * nested class with default member initializers as a default argument in its enclosing class.
     */
    struct FakeSystemExtra
    {
        StringView TypeName{};
        bool Spawns = false;
        bool RawStorage = false;
    };

    /** Fake and real system descriptions. Holds the access rows so the spans stay valid. */
    class FakeSystems
    {
    public:
        /** One row of a fake access list, with the kind and domain that a real AccessEntry has. */
        struct Use
        {
            const char* Type;
            AccessMode Mode;
            ComponentKind Kind = Table;
            ComponentDomain Domain = ComponentDomain::Simulation;
            RollbackClass Rollback = RollbackClass::Restore;
            /** On a Read row: the system also blends the type (ReadBlend). */
            bool Blend = false;
        };

        using Extra = FakeSystemExtra;

        FakeSystems& Add(SystemInfo info, std::vector<Use> uses, const Extra extra = {})
        {
            std::vector<AccessEntry>& rows = m_Rows.emplace_back();
            for (const Use& use : uses)
            {
                rows.push_back(AccessEntry{
                    HashName(use.Type), use.Mode, use.Type, use.Kind, use.Domain, true, use.Rollback, use.Blend});
            }
            m_Descs.push_back(SystemDesc{.TypeName = extra.TypeName,
                                         .Info = info,
                                         .Access = rows,
                                         .Spawns = extra.Spawns,
                                         .RawStorage = extra.RawStorage});
            m_Pointers.push_back(&m_Descs.back());
            return *this;
        }

        /** Adds a fake system that SystemOf<T> finds, with T's name as its TypeName. T can be a forward declaration. */
        template <typename T>
        FakeSystems& Add(SystemInfo info, std::vector<Use> uses)
        {
            return Add(info, std::move(uses), Extra{.TypeName = TypeNameOf<T>()});
        }

        /** Adds the description of a real system class. */
        template <typename T>
        FakeSystems& Real()
        {
            return Desc(Detail::DescOf<T, typename T::Access>);
        }

        /** Adds a description that the caller keeps alive. */
        FakeSystems& Desc(const SystemDesc& desc)
        {
            m_Pointers.push_back(&desc);
            return *this;
        }

        /** Adds a group that nothing refers to. The group must outlive the build. */
        FakeSystems& AddGroup(const GroupInfo& group)
        {
            m_Groups.push_back(&group);
            return *this;
        }

        FakeSystems& RunBefore(const OrderRef first, const OrderRef second)
        {
            m_Orders.push_back(ScheduleOrder{first, second});
            return *this;
        }

        /** Builds with the systems in the order they were added, or in the reverse order. */
        ScheduleResult Build(const bool reversed = false) const
        {
            std::vector<const SystemDesc*> pointers = m_Pointers;
            if (reversed)
            {
                std::reverse(pointers.begin(), pointers.end());
            }
            return BuildSchedule(ScheduleInput{pointers, m_Groups, m_Orders});
        }

    private:
        std::deque<std::vector<AccessEntry>> m_Rows;
        std::deque<SystemDesc> m_Descs;
        std::vector<const SystemDesc*> m_Pointers;
        std::vector<const GroupInfo*> m_Groups;
        std::vector<ScheduleOrder> m_Orders;
    };

    constexpr AccessMode R = AccessMode::Read;
    constexpr AccessMode W = AccessMode::Write;
    constexpr AccessMode A = AccessMode::Append;
    constexpr AccessMode C = AccessMode::Consume;

    SystemInfo Sim(const char* name)
    {
        return SystemInfo{.Name = name, .In = Order::Simulation};
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

    /** The names of a list of system indices. */
    std::vector<String> Names(const Schedule& schedule, const std::vector<uint32>& systems)
    {
        std::vector<String> names;
        for (const uint32 i : systems)
        {
            names.emplace_back(schedule.Systems[i]->Info.Name);
        }
        return names;
    }

    /** The system names of one update rate in run order. */
    std::vector<String> RunNames(const Schedule& schedule, const UpdateRate rate = UpdateRate::Fixed)
    {
        return Names(schedule, schedule.RunOrder(rate));
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

    std::size_t CountErrors(const ScheduleResult& result, const ScheduleErrorKind kind)
    {
        return static_cast<std::size_t>(std::count_if(
            result.Errors.begin(), result.Errors.end(), [kind](const ScheduleError& e) { return e.Kind == kind; }));
    }

    /** The system names of the only error, which must be of the kind. */
    std::vector<String> OnlyError(const ScheduleResult& result, const ScheduleErrorKind kind)
    {
        INFO(result.ErrorReport());
        REQUIRE(result.Errors.size() == 1);
        REQUIRE(result.Errors[0].Kind == kind);
        return Names(result.Value, result.Errors[0].Systems);
    }

    using Stages = std::vector<std::vector<String>>;

    /**
     * A fake system has no class, but SystemOf<T> needs one. Each fake system that an
     * order names gets a tag type here, and is added with FakeSystems::Add<Tag::X>.
     */
    namespace Tag
    {
        struct A;
        struct B;
        struct C;
        struct Rotator;
        struct Mover;
        struct Reader;
        struct Writer;
        struct Draw;
        struct ModAMover;
        struct ModBMover;
        struct Gameplay;
        struct Late;
        struct Nobody;
    } // namespace Tag
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
    REQUIRE(OnlyError(result, ScheduleErrorKind::WriteConflict) == std::vector<String>{"Test.Mover", "Test.Rotator"});
    // A description made by hand has no class to name in the fix
    const String& message = result.Errors[0].Message;
    REQUIRE(message.find("Test.Mover and Test.Rotator both write Transform") != String::npos);
    REQUIRE(message.find("Fix: add Test.Mover to the .After of Test.Rotator (or the reverse)") != String::npos);
}

TEST_CASE("An order through a third system orders two writers", "[ecs][schedule]")
{
    // Spawn before Rotator, and Mover after Rotator: Spawn and Mover are ordered too
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Spawn", .In = Order::Simulation, .Before = SystemOf<Tag::Rotator>},
                {{"Transform", W}});
    systems.Add<Tag::Rotator>(Sim("Test.Rotator"), {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Mover", .In = Order::Simulation, .After = SystemOf<Tag::Rotator>},
                {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Spawn", "Test.Rotator", "Test.Mover"});
}

TEST_CASE("Two writers or consumers in worlds that never meet are not an error", "[ecs][schedule]")
{
    for (const AccessMode mode : {W, C})
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.EditTool", .In = Order::ResolveDeferred, .RunIn = WorldRole::Edit},
                    {{"Q", mode}});
        systems.Add(SystemInfo{.Name = "Test.Play", .In = Order::ResolveDeferred, .RunIn = WorldRole::Play},
                    {{"Q", mode}});
        REQUIRE(systems.Build().ErrorReport() == "");

        // The same pair in worlds that meet
        FakeSystems meeting;
        meeting.Add(SystemInfo{.Name = "Test.EditTool", .In = Order::ResolveDeferred, .RunIn = WorldRole::All},
                    {{"Q", mode}});
        meeting.Add(SystemInfo{.Name = "Test.Play", .In = Order::ResolveDeferred, .RunIn = WorldRole::Play},
                    {{"Q", mode}});
        REQUIRE(OnlyError(meeting.Build(),
                          mode == W ? ScheduleErrorKind::WriteConflict : ScheduleErrorKind::ConsumeConflict) ==
                std::vector<String>{"Test.EditTool", "Test.Play"});
    }
}

TEST_CASE("After and Before order two writers", "[ecs][schedule]")
{
    SECTION("After")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .After = SystemOf<Tag::B>}, {{"X", W}});
        systems.Add<Tag::B>(Sim("Test.B"), {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.B"}, {"Test.A"}});
        REQUIRE(result.Value.Edges[0].Reason == EdgeReason::After);
    }

    SECTION("Before")
    {
        FakeSystems systems;
        systems.Add<Tag::A>(Sim("Test.A"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.B", .In = Order::Simulation, .Before = SystemOf<Tag::A>}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.B"}, {"Test.A"}});
    }

    SECTION("An explicit order replaces the inferred edge")
    {
        // The writer would run first, but .After puts the reader first.
        FakeSystems systems;
        systems.Add<Tag::Reader>(Sim("Test.Reader"), {{"X", R}});
        systems.Add(SystemInfo{.Name = "Test.Writer", .In = Order::Simulation, .After = SystemOf<Tag::Reader>},
                    {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.Ok());
        REQUIRE(StageNames(result.Value) == Stages{{"Test.Reader"}, {"Test.Writer"}});
    }

    SECTION("A list of systems")
    {
        FakeSystems systems;
        systems.Add(
            SystemInfo{.Name = "Test.Last", .In = Order::Simulation, .After = {SystemOf<Tag::A>, SystemOf<Tag::B>}},
            {});
        systems.Add<Tag::A>(Sim("Test.A"), {});
        systems.Add<Tag::B>(Sim("Test.B"), {});

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
    REQUIRE(error.Message.find("Test.A before Test.C (it writes X)") != String::npos);
    REQUIRE(error.Message.find("Test.B before Test.A (it writes Y)") != String::npos);
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

TEST_CASE("The run order is by stage, then by name", "[ecs][schedule]")
{
    // Test.Z has no edge, so it is in stage 0 with Test.A, before Test.B, whatever its name
    for (const bool reversed : {false, true})
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {{"X", W}});
        systems.Add(Sim("Test.B"), {{"X", R}});
        systems.Add(Sim("Test.Z"), {{"Y", W}});
        const ScheduleResult result = systems.Build(reversed);
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(StageNames(result.Value) == Stages{{"Test.A", "Test.Z"}, {"Test.B"}});
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.A", "Test.Z", "Test.B"});
    }
}

TEST_CASE("Bad names are errors", "[ecs][schedule]")
{
    SECTION("After names a system of the other update rate")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .After = SystemOf<Tag::Draw>}, {});
        systems.Add<Tag::Draw>(SystemInfo{.Name = "Test.Draw", .In = Order::Render}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::OrderAcrossUpdateRates));
    }

    SECTION("No group")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A"}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::MissingGroup));
    }

    SECTION("Duplicate name")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {});
        systems.Add(SystemInfo{.Name = "Test.A", .In = Order::Render}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::DuplicateName));
    }

    SECTION("No name")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.In = Order::Simulation}, {});
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::MissingName));
    }
}

TEST_CASE("A queue has one consumer", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.ResolveA", .In = Order::ResolveDeferred}, {{"Q", C}});
    systems.Add(SystemInfo{.Name = "Test.ResolveB", .In = Order::ResolveDeferred}, {{"Q", C}});
    REQUIRE(HasError(systems.Build(), ScheduleErrorKind::ConsumeConflict));
}

TEST_CASE("Groups keep their own stages", "[ecs][schedule]")
{
    // A writer in a later group does not make an edge to a reader in an earlier one.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Publish", .In = Order::Render}, {{"X", R}});
    systems.Add(SystemInfo{.Name = "Test.Input", .In = Order::Input}, {{"Input", W, Table, SimDomain, Rebuild}});
    systems.Add(Sim("Test.Move"), {{"Input", R, Table, SimDomain, Rebuild}, {"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(result.Value.Edges.empty());

    std::vector<String> run;
    for (const uint32 i : result.Value.RunOrder())
    {
        run.emplace_back(result.Value.Systems[i]->Info.Name);
    }
    REQUIRE(run == std::vector<String>{"Test.Input", "Test.Move", "Test.Publish"});
}

TEST_CASE("Dump lists groups, stages, writes, and reasons", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Input", .In = Order::Input}, {{"Actions", W, Table, SimDomain, Rebuild}});
    systems.Add<Tag::Mover>(Sim("Sandbox.Mover"), {{"Mover", W}, {"Transform", W}});
    systems.Add(SystemInfo{.Name = "Sandbox.Rotator", .In = Order::Simulation, .After = SystemOf<Tag::Mover>},
                {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());

    const String dump = result.Value.Dump();
    REQUIRE(dump.find("Fixed (once per tick)\n") != String::npos);
    REQUIRE(dump.find("  groups:\n"
                      "    Engine.Tick\n"
                      "      Engine.Input\n"
                      "      Engine.Simulation  (resim)\n"
                      "      Engine.Physics  (resim)\n"
                      "      Engine.ResolveDeferred  (resim)\n"
                      "      Engine.Finalize\n"
                      "  stage 0") != String::npos);
    REQUIRE(dump.find("Variable (once per frame)") == String::npos);
    REQUIRE(dump.find("[Engine.Simulation]") != String::npos);
    REQUIRE(dump.find("writes Mover, Transform") != String::npos);
    REQUIRE(StageNames(result.Value) == Stages{{"Test.Input"}, {"Sandbox.Mover"}, {"Sandbox.Rotator"}});
    REQUIRE(dump.find("after Sandbox.Mover: .After") != String::npos);
    REQUIRE(dump.find("flush: end of tick") != String::npos);
}

TEST_CASE("The schedule lists the singletons that no system writes", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "T.Input", .In = Order::Input}, {{"Score", W, Single, SimDomain, Rebuild}});
    systems.Add(Sim("T.Move"), {{"SimTime", R, Single}, {"Score", R, Single}, {"Tuning", R, Single}, {"X", W}});
    systems.Add(SystemInfo{.Name = "F.Show", .In = Order::Presentation},
                {{"Tuning", R, Single}, {"Theme", R, Single, Pres}, {"View", W, Table, Pres}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;

    // Not Score (T.Input writes it), not SimTime (the runner writes it). Sorted by name.
    REQUIRE(schedule.ReadOnlySingletons.size() == 2);
    CHECK(String(schedule.ReadOnlySingletons[0].Name) == "Theme");
    CHECK(Names(schedule, schedule.ReadOnlySingletons[0].Readers) == std::vector<String>{"F.Show"});
    CHECK(String(schedule.ReadOnlySingletons[1].Name) == "Tuning");
    CHECK(Names(schedule, schedule.ReadOnlySingletons[1].Readers) == std::vector<String>{"F.Show", "T.Move"});
    CHECK(schedule.Dump().find("Singletons that no system writes (the runner makes each with its default values, "
                               "unless it is External)\n"
                               "  Theme (singleton), read by F.Show\n"
                               "  Tuning (singleton), read by F.Show, T.Move\n") != String::npos);

    FakeSystems written;
    written.Add(SystemInfo{.Name = "T.Input", .In = Order::Input}, {{"Score", W, Single, SimDomain, Rebuild}});
    written.Add(Sim("T.Move"), {{"Score", R, Single}, {"X", W}});
    const ScheduleResult none = written.Build();
    REQUIRE(none.ErrorReport() == "");
    CHECK(none.Value.ReadOnlySingletons.empty());
    CHECK(none.Value.Dump().find("no system writes") == String::npos);
}

// ---- Groups of other modules (Plans/ecs-systems.md 3.8) --------------------------------------

namespace
{
    // Two modules that do not know each other. Both run after Simulation and before ResolveDeferred,
    // and are resimulated (they write game state). Each group is in Engine.Tick or Engine.Frame: only
    // those two are roots.
    inline constexpr GroupInfo ModAGroup{.Name = "ModA.Abilities",
                                         .In = Order::Tick,
                                         .After = Order::Simulation,
                                         .Before = Order::ResolveDeferred,
                                         .Resim = true};
    inline constexpr GroupInfo ModBGroup{.Name = "ModB.Combo",
                                         .In = Order::Tick,
                                         .After = Order::Simulation,
                                         .Before = Order::ResolveDeferred,
                                         .Resim = true};

    inline constexpr GroupInfo LateGroup{.Name = "Game.Late", .In = Order::Tick, .After = Order::ResolveDeferred};
    // Between two resimulated groups, so it must be resimulated too (ScheduleErrorKind::ResimGap)
    inline constexpr GroupInfo FlushingGroup{.Name = "Game.Flush",
                                             .In = Order::Tick,
                                             .After = Order::Simulation,
                                             .Before = Order::ResolveDeferred,
                                             .FlushAfter = true,
                                             .Resim = true};
    inline constexpr GroupInfo LooseFlushGroup{
        .Name = "Game.LooseFlush", .In = Order::Tick, .After = Order::Input, .FlushAfter = true};
    inline constexpr GroupInfo WrongRateGroup{.Name = "Game.WrongRate", .In = Order::Tick, .After = Order::Render};
    // A group can only name a group defined above it, so RunBefore closes this loop.
    inline constexpr GroupInfo LoopA{.Name = "Game.LoopA", .In = Order::Tick};
    inline constexpr GroupInfo LoopB{.Name = "Game.LoopB", .In = Order::Tick, .After = LoopA};
    inline constexpr GroupInfo FrameGroupWithFlush{.Name = "Game.FrameFlush", .In = Order::Frame, .FlushAfter = true};
    inline constexpr GroupInfo SameNameAsModA{.Name = "ModA.Abilities", .In = Order::Frame};

    // Groups with no parent
    inline constexpr GroupInfo NoParent{.Name = "Game.NoParent"};
    inline constexpr GroupInfo InNoParent{.Name = "Game.InNoParent", .In = NoParent};
    static_assert(!Detail::IsRooted(NoParent) && !Detail::IsRooted(InNoParent));
    static_assert(Detail::IsRooted(Order::Tick) && Detail::IsRooted(Order::Physics) && Detail::IsRooted(ModAGroup));
} // namespace

TEST_CASE("Two modules' groups are ordered by the data they share", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "ModB.ComboSystem", .In = ModBGroup}, {{"Cooldown", R}});
    systems.Add(SystemInfo{.Name = "ModA.CooldownSystem", .In = ModAGroup}, {{"Cooldown", W}});
    systems.Add(Sim("Engine.Gameplay"), {{"Cooldown", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");

    // Gameplay is in Simulation, which both groups follow. ModA writes what ModB reads.
    REQUIRE(RunNames(result.Value) ==
            std::vector<String>{"Engine.Gameplay", "ModA.CooldownSystem", "ModB.ComboSystem"});
    REQUIRE(result.Value.Edges.size() == 1);
    REQUIRE(result.Value.Edges[0].Reason == EdgeReason::WriteRead);
}

TEST_CASE("Unrelated groups with no shared data share a stage", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "ModA.Thing", .In = ModAGroup}, {{"X", W}});
    systems.Add(SystemInfo{.Name = "ModB.Other", .In = ModBGroup}, {{"Y", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());
    REQUIRE(StageNames(result.Value) == Stages{{"ModA.Thing", "ModB.Other"}});
}

TEST_CASE("Two unordered groups that write one type are an error", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "ModA.Mover", .In = ModAGroup}, {{"Transform", W}});
    systems.Add(SystemInfo{.Name = "ModB.Mover", .In = ModBGroup}, {{"Transform", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(OnlyError(result, ScheduleErrorKind::WriteConflict) == std::vector<String>{"ModA.Mover", "ModB.Mover"});
    REQUIRE(HasText(result, "group ModA.Abilities and group ModB.Combo"));
}

TEST_CASE("RunBefore from outside fixes a conflict between two modules", "[ecs][schedule][groups]")
{
    SECTION("Order two groups")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "ModA.Mover", .In = ModAGroup}, {{"Transform", W}});
        systems.Add(SystemInfo{.Name = "ModB.Mover", .In = ModBGroup}, {{"Transform", W}});
        systems.RunBefore(ModBGroup, ModAGroup);

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"ModB.Mover", "ModA.Mover"});
    }

    SECTION("Order two systems")
    {
        FakeSystems systems;
        systems.Add<Tag::ModAMover>(SystemInfo{.Name = "ModA.Mover", .In = ModAGroup}, {{"Transform", W}});
        systems.Add<Tag::ModBMover>(SystemInfo{.Name = "ModB.Mover", .In = ModBGroup}, {{"Transform", W}});
        systems.RunBefore(SystemOf<Tag::ModBMover>, SystemOf<Tag::ModAMover>);

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"ModB.Mover", "ModA.Mover"});
        REQUIRE(result.Value.Edges[0].Reason == EdgeReason::RunBefore);
    }

    SECTION("RunBefore of a system that was not added")
    {
        FakeSystems systems;
        systems.RunBefore(ModAGroup, SystemOf<Tag::Nobody>);
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::OrderTargetNotAdded).empty());
    }

    SECTION("RunBefore with an empty reference")
    {
        FakeSystems systems;
        systems.RunBefore(ModAGroup, OrderRef{});
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::EmptyOrderRef).empty());
    }
}

TEST_CASE("Group order wins over a data edge that points back", "[ecs][schedule][groups]")
{
    // Late writes X after ResolveDeferred. A reader in Input, which is earlier, sees the
    // value from the previous tick. That is not an edge and not a cycle. (Neither is
    // resimulated: a resimulated reader of X is ScheduleErrorKind::ResimSkipsState.)
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Game.LateWriter", .In = LateGroup}, {{"X", W, Table, SimDomain, Rebuild}});
    systems.Add(SystemInfo{.Name = "Engine.Reader", .In = Order::Input}, {{"X", R, Table, SimDomain, Rebuild}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(result.Value.Edges.empty());
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Engine.Reader", "Game.LateWriter"});
}

TEST_CASE("A read that runs before its writer is listed as one tick late", "[ecs][schedule]")
{
    // In one group the writer runs first, so nothing is late
    FakeSystems inOrder;
    inOrder.Add(Sim("T.Reader"), {{"Health", R}}).Add(Sim("T.Writer"), {{"Health", W}});
    REQUIRE(inOrder.Build().Value.Lags.empty());

    // Group order puts the reader and the consumer first. Their data is from the tick before.
    FakeSystems late;
    late.Add(Sim("T.Reader"), {{"Contacts", R}})
        .Add(Sim("T.Consumer"), {{"Hits", C}})
        .Add(SystemInfo{.Name = "T.Step", .In = Order::ResolveDeferred}, {{"Contacts", W}, {"Hits", A}});
    const ScheduleResult result = late.Build();
    REQUIRE(result.ErrorReport() == "");
    const std::vector<ScheduleLag>& lags = result.Value.Lags;
    REQUIRE(lags.size() == 2);
    REQUIRE(String(result.Value.Systems[lags[0].Late]->Info.Name) == "T.Consumer");
    REQUIRE(lags[0].Queue);
    REQUIRE(String(lags[0].TypeName) == "Hits");
    REQUIRE(String(result.Value.Systems[lags[1].Late]->Info.Name) == "T.Reader");
    REQUIRE_FALSE(lags[1].Queue);
    REQUIRE(String(result.Value.Systems[lags[1].Source]->Info.Name) == "T.Step");

    const String dump = result.Value.Dump();
    REQUIRE(dump.find("one tick late: reads Contacts before T.Step writes it") != String::npos);
    REQUIRE(dump.find("one tick late: consumes Hits before T.Step appends to it") != String::npos);

    // An explicit order can do the same inside one group
    FakeSystems chosen;
    chosen
        .Add(SystemInfo{.Name = "T.Reader", .In = Order::Simulation, .Before = SystemOf<Tag::Writer>}, {{"Health", R}})
        .Add<Tag::Writer>(Sim("T.Writer"), {{"Health", W}});
    REQUIRE(chosen.Build().Value.Lags.size() == 1);
}

TEST_CASE("A system After can cross groups of one update rate", "[ecs][schedule][groups]")
{
    SECTION("It agrees with group order")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Game.Late", .In = LateGroup, .After = SystemOf<Tag::Gameplay>}, {});
        systems.Add<Tag::Gameplay>(Sim("Engine.Gameplay"), {});
        REQUIRE(systems.Build().Ok());
    }

    SECTION("It contradicts group order, which is a cycle")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Engine.Gameplay", .In = Order::Simulation, .After = SystemOf<Tag::Late>}, {});
        systems.Add<Tag::Late>(SystemInfo{.Name = "Game.Late", .In = LateGroup}, {});

        const ScheduleResult result = systems.Build();
        REQUIRE(HasError(result, ScheduleErrorKind::Cycle));
        REQUIRE(HasText(result, "Game.Late before Engine.Gameplay (.After)"));
    }
}

TEST_CASE("Group declaration errors", "[ecs][schedule][groups]")
{
    SECTION("A fixed group placed against a variable rate group")
    {
        FakeSystems systems;
        systems.AddGroup(WrongRateGroup);
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::OrderAcrossUpdateRates).empty());
    }

    SECTION("Groups in a loop")
    {
        FakeSystems systems;
        systems.AddGroup(LoopA).AddGroup(LoopB).RunBefore(LoopB, LoopA);
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::Cycle).empty());
        REQUIRE(HasText(result, "Game.LoopA (end) -> Game.LoopB (begin)"));
    }

    SECTION("Two different groups with one name")
    {
        FakeSystems systems;
        systems.AddGroup(ModAGroup).AddGroup(SameNameAsModA);
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::DuplicateGroup).empty());
        REQUIRE(HasText(result, "Two different groups have the name \"ModA.Abilities\": their .In differs."));
    }

    SECTION("FlushAfter on a variable rate group")
    {
        FakeSystems systems;
        systems.AddGroup(FrameGroupWithFlush);
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::FlushInVariableGroup).empty());
    }

    SECTION("A group with no name names where it is used")
    {
        static constexpr GroupInfo nameless{.In = Order::Simulation};
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .In = nameless}, {});
        const ScheduleResult result = systems.Build();
        REQUIRE(HasError(result, ScheduleErrorKind::MissingName));
        REQUIRE(HasText(result, "A group in the .In of Test.A has no name."));
    }
}

TEST_CASE("Every group except the two roots sets In", "[ecs][schedule][groups]")
{
    SECTION("A group with no In")
    {
        FakeSystems systems;
        systems.AddGroup(NoParent);
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::MissingParent).empty());
        REQUIRE(HasText(result, "Group Game.NoParent has no .In. Put it in a group"));
    }

    SECTION("A system in a child of a group with no In")
    {
        // A SystemDesc made by hand skips CheckSystem, which makes this a compile error.
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Lost", .In = InNoParent}, {});
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::MissingParent).empty());
    }

    SECTION("A group with the name of a root and an In is a second group with that name")
    {
        static constexpr GroupInfo fakeTick{.Name = "Engine.Tick", .In = Order::Frame};
        FakeSystems systems;
        systems.AddGroup(fakeTick);
        REQUIRE(HasError(systems.Build(), ScheduleErrorKind::DuplicateGroup));
    }
}

TEST_CASE("An order list made at run time with more than eight items is an error", "[ecs][schedule][groups]")
{
    // Not a constant, so the list compiles, keeps eight items, and remembers that it lost some.
    const GroupInfo* input = &Order::Input;
    const OrderList tooLong{*input, *input, *input, *input, *input, *input, *input, *input, *input};
    REQUIRE(tooLong.TooMany());
    REQUIRE(tooLong.Size() == OrderList::Capacity);

    const SystemInfo info{.Name = "Test.Long", .In = Order::Simulation, .After = tooLong};
    FakeSystems systems;
    systems.Add(info, {});
    const ScheduleResult result = systems.Build();
    REQUIRE(OnlyError(result, ScheduleErrorKind::OrderListTooLong) == std::vector<String>{"Test.Long"});
    REQUIRE(HasText(result, "Test.Long has .After with more than 8 items"));

    const GroupInfo group{.Name = "Game.Long", .In = Order::Tick, .Before = tooLong};
    FakeSystems withGroup;
    withGroup.AddGroup(group);
    REQUIRE(HasText(withGroup.Build(), "Group Game.Long has .Before with more than 8 items"));

    constexpr OrderList full{
        Order::Input, Order::Input, Order::Input, Order::Input, Order::Input, Order::Input, Order::Input, Order::Input};
    STATIC_REQUIRE_FALSE(full.TooMany());
}

TEST_CASE("An empty reference inside an order list made at run time is an error", "[ecs][schedule][groups]")
{
    // Not a constant, so the list compiles. Every item is kept, so B is not lost.
    const GroupInfo* input = &Order::Input;
    const OrderList gap{*input, OrderRef{}, SystemOf<Tag::B>};
    REQUIRE(gap.Size() == 3);

    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .After = gap}, {});
    systems.Add<Tag::B>(Sim("Test.B"), {});
    const ScheduleResult result = systems.Build();
    REQUIRE(OnlyError(result, ScheduleErrorKind::EmptyOrderRef) == std::vector<String>{"Test.A"});
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.B", "Test.A"});
}

TEST_CASE("A FlushAfter group splits the tick into segments", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.Add(Sim("Engine.Gameplay"), {});
    systems.Add(SystemInfo{.Name = "Game.Flushing", .In = FlushingGroup}, {});
    systems.Add(SystemInfo{.Name = "Engine.Resolve", .In = Order::ResolveDeferred}, {});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");

    const std::vector<ScheduleSegment>& segments = result.Value.GetSegments(UpdateRate::Fixed);
    REQUIRE(segments.size() == 2);
    REQUIRE(segments[0].FlushGroup == &FlushingGroup);
    REQUIRE(segments[1].FlushGroup == nullptr);
    REQUIRE(StageNames(result.Value) == Stages{{"Engine.Gameplay"}, {"Game.Flushing"}, {"Engine.Resolve"}});
    REQUIRE(result.Value.Dump().find("flush: end of group Game.Flush") != String::npos);
}

TEST_CASE("A FlushAfter group must be ordered against every group with systems", "[ecs][schedule][groups]")
{
    // LooseFlush is only after Input, so nothing orders it against ModA's group.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Game.Flushing", .In = LooseFlushGroup}, {});
    systems.Add(SystemInfo{.Name = "ModA.Thing", .In = ModAGroup}, {});

    const ScheduleResult result = systems.Build();
    INFO(result.ErrorReport());
    REQUIRE(result.Errors.size() == 2);
    REQUIRE(CountErrors(result, ScheduleErrorKind::FlushNotOrdered) == 1);
    REQUIRE(HasText(result, "Game.LooseFlush has .FlushAfter, but nothing orders it against group ModA.Abilities"));
    // ModA.Abilities is resimulated, and nothing orders Game.Flushing against it either
    REQUIRE(CountErrors(result, ScheduleErrorKind::ResimGap) == 1);
}

TEST_CASE("Groups are listed in run order", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.AddGroup(LateGroup).AddGroup(ModAGroup);

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Ok());

    std::vector<String> names;
    for (const ScheduleGroup& group : result.Value.Groups)
    {
        names.emplace_back(group.Info->Name);
    }
    REQUIRE(names == std::vector<String>{"Engine.Tick",
                                         "Engine.Input",
                                         "Engine.Simulation",
                                         "Engine.Physics",
                                         "ModA.Abilities",
                                         "Engine.ResolveDeferred",
                                         "Engine.Finalize",
                                         "Game.Late",
                                         "Engine.Frame",
                                         "Engine.Presentation",
                                         "Engine.Render",
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
        static constexpr SystemInfo Info{.Name = "Test.ScoreWriter", .In = Order::Simulation};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class ScoreReader final : public System<ScoreReader, Access<Read<Score>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.ScoreReader", .In = Order::Simulation};

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

// ---- Nested groups and typed order references ------------------------------------------------

namespace
{
    // Forward declarations only: SystemOf<T> needs no more than this.
    class FakeStep;
    class FakeNotAdded;

    inline constexpr GroupInfo Inner{.Name = "Game.Inner", .In = Order::Simulation};
    inline constexpr GroupInfo Deeper{.Name = "Game.Deeper", .In = Inner};
    inline constexpr GroupInfo Deepest{.Name = "Game.Deepest", .In = Deeper};
    inline constexpr GroupInfo BeforeStep{
        .Name = "Game.BeforeStep", .In = Order::Physics, .Before = SystemOf<FakeStep>};
    inline constexpr GroupInfo BeforeMissing{
        .Name = "Game.BeforeMissing", .In = Order::Physics, .Before = SystemOf<FakeNotAdded>};
    inline constexpr GroupInfo FlushInner{.Name = "Game.FlushInner", .In = Order::Simulation, .FlushAfter = true};
    inline constexpr GroupInfo LoopFirst{.Name = "Game.LoopFirst", .In = Order::Simulation};
    inline constexpr GroupInfo LoopSecond{.Name = "Game.LoopSecond", .In = Order::Simulation, .After = LoopFirst};

    // Two copies of one group, as two shared libraries hold two copies of one inline variable.
    inline constexpr GroupInfo CopyA{.Name = "Game.Copy", .In = Order::Simulation};
    inline constexpr GroupInfo CopyB{.Name = "Game.Copy", .In = Order::Simulation};

    static_assert(RateOf(Deepest) == UpdateRate::Fixed);
    static_assert(RateOf(Order::Editor) == UpdateRate::Variable);

    uint32 GroupIndex(const Schedule& schedule, const StringView name)
    {
        for (uint32 g = 0; g < schedule.Groups.size(); ++g)
        {
            if (StringView(schedule.Groups[g].Info->Name) == name)
            {
                return g;
            }
        }
        return Schedule::NoGroup;
    }
} // namespace

TEST_CASE("OrderList takes one reference or a mixed braced list", "[ecs][schedule][groups]")
{
    constexpr OrderList mixed{SystemOf<FakeStep>, Order::Input, SystemOf<FakeNotAdded>};
    STATIC_REQUIRE(mixed.Size() == 3);
    STATIC_REQUIRE(mixed[0].IsSystem());
    STATIC_REQUIRE(mixed[1].IsGroup());
    STATIC_REQUIRE(mixed[2].IsSystem());

    constexpr OrderList system = SystemOf<FakeStep>;
    constexpr OrderList group = Order::Input;
    constexpr OrderList none{};
    STATIC_REQUIRE(system.Size() == 1);
    STATIC_REQUIRE(group.Size() == 1);
    STATIC_REQUIRE(none.Size() == 0);
    STATIC_REQUIRE(group[0].Group() == &Order::Input);

    // FakeStep is only declared. Its name gives the id that the complete class has.
    STATIC_REQUIRE(OrderRef(SystemOf<FakeStep>).SystemTypeName().ends_with("FakeStep"));
    STATIC_REQUIRE(Detail::SystemTypeHash(OrderRef(SystemOf<FakeStep>).SystemTypeName()) == SystemTypeId<FakeStep>());
}

TEST_CASE("A group can be inside an engine group, to any depth", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Input", .In = Order::Input}, {{"In", W, Table, SimDomain, Rebuild}});
    systems.Add(SystemInfo{.Name = "Test.Deepest", .In = Deepest}, {{"In", R, Table, SimDomain, Rebuild}, {"X", W}});
    systems.Add(SystemInfo{.Name = "Test.AStep", .In = Order::Physics}, {{"X", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Input", "Test.Deepest", "Test.AStep"});
    // The groups order every pair, so there is no data edge and no late read.
    REQUIRE(result.Value.Edges.empty());
    REQUIRE(result.Value.Lags.empty());

    const Schedule& schedule = result.Value;
    const uint32 deepest = GroupIndex(schedule, "Game.Deepest");
    REQUIRE(deepest != Schedule::NoGroup);
    REQUIRE(schedule.Groups[deepest].Rate == UpdateRate::Fixed);
    std::vector<String> parents;
    for (uint32 g = schedule.Groups[deepest].Parent; g != Schedule::NoGroup; g = schedule.Groups[g].Parent)
    {
        parents.emplace_back(schedule.Groups[g].Info->Name);
    }
    REQUIRE(parents == std::vector<String>{"Game.Deeper", "Game.Inner", "Engine.Simulation", "Engine.Tick"});
    REQUIRE(schedule.SystemGroup[1] == deepest);

    const String dump = schedule.Dump();
    REQUIRE(dump.find("[Game.Deepest]") != String::npos);
    REQUIRE(dump.find("      Engine.Simulation  (resim)\n"
                      "        Game.Inner  (resim)\n"
                      "          Game.Deeper  (resim)\n"
                      "            Game.Deepest  (resim)\n"
                      "      Engine.Physics  (resim)\n") != String::npos);
}

namespace
{
    class SuspensionSystem;
    class SteeringSystem;
    class VehicleStepSystem;

    struct HoverBody
    {
        static constexpr ComponentInfo Component{.Name = "GroupsTest.HoverBody"};
        float Lift = 0.0f;
    };

    /** Stands in for Transform, which the hover system and the physics step both write. */
    struct HoverPose
    {
        static constexpr ComponentInfo Component{.Name = "GroupsTest.HoverPose"};
        float Height = 0.0f;
    };

    // The shape of section 8 of the design: vehicle work inside Physics, before the step.
    inline constexpr GroupInfo Vehicles{
        .Name = "Game.Vehicles", .In = Order::Physics, .Before = SystemOf<VehicleStepSystem>};

    class SuspensionSystem final : public System<SuspensionSystem, Access<Write<HoverBody>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Game.Suspension", .In = Vehicles};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class SteeringSystem final : public System<SteeringSystem, Access<Read<HoverBody>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Game.Steering", .In = Vehicles};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class HoverSystem final : public System<HoverSystem, Access<Read<HoverBody>, Write<HoverPose>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Game.Hover", .In = Vehicles, .After = {SystemOf<SuspensionSystem>, SystemOf<SteeringSystem>}};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class VehicleStepSystem final : public System<VehicleStepSystem, Access<Write<HoverPose>>>
    {
    public:
        // "A.Step" sorts before every vehicle system, so only the group order puts it last.
        static constexpr SystemInfo Info{.Name = "A.Step", .In = Order::Physics};

        void FixedUpdate(Context& /*ctx*/) const {}
    };
} // namespace

TEST_CASE("A nested group runs before a system of its parent through SystemOf", "[ecs][schedule][groups]")
{
    SystemRegistry registry;
    registry.Add<VehicleStepSystem>();
    registry.Add<HoverSystem>();
    registry.Add<SteeringSystem>();
    registry.Add<SuspensionSystem>();
    REQUIRE(Detail::SystemTypeHash(registry.Descs()[1]->TypeName) == SystemTypeId<HoverSystem>());

    const ScheduleResult result = registry.BuildSchedule();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(StageNames(result.Value) == Stages{{"Game.Suspension"}, {"Game.Steering"}, {"Game.Hover"}, {"A.Step"}});

    // Hover and the step both write HoverPose. The group order puts one before the other: no conflict, no edge.
    for (const ScheduleEdge& edge : result.Value.Edges)
    {
        REQUIRE(String(result.Value.Systems[edge.To]->Info.Name) != "A.Step");
    }

    const String dump = result.Value.Dump();
    REQUIRE(dump.find("      Engine.Physics  (resim)\n        Game.Vehicles  (resim)\n") != String::npos);
    REQUIRE(dump.find("[Game.Vehicles]") != String::npos);
    REQUIRE(dump.find("after Game.Suspension: .After") != String::npos);
    REQUIRE(dump.find("after Game.Steering: .After") != String::npos);
}

TEST_CASE("A system can be ordered against a group, and a group against a system", "[ecs][schedule][groups]")
{
    // Every section picks names that sort the other way, so only the order makes the result.
    SECTION("A system after a group")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.InInner", .In = Inner}, {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.AfterInner", .In = Order::Simulation, .After = Inner}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.InInner", "Test.AfterInner"});
        // An order against a group is not a ScheduleEdge, which joins two systems.
        REQUIRE(result.Value.Edges.empty());
    }

    SECTION("A system before a group")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.AInner", .In = Inner}, {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Zed", .In = Order::Simulation, .Before = Inner}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Zed", "Test.AInner"});
    }

    SECTION("A group before a system")
    {
        FakeSystems systems;
        systems.Add<FakeStep>(SystemInfo{.Name = "Test.AStep", .In = Order::Physics}, {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Zed", .In = BeforeStep}, {{"X", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Zed", "Test.AStep"});
    }

    SECTION("RunBefore between a system and a group")
    {
        FakeSystems systems;
        systems.Add<FakeStep>(SystemInfo{.Name = "Test.Zed", .In = Order::Simulation}, {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.AInner", .In = Inner}, {{"X", W}});
        systems.RunBefore(SystemOf<FakeStep>, Inner);

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Zed", "Test.AInner"});
    }

    SECTION("An order between a variable rate system and a fixed group")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Draw", .In = Order::Presentation, .After = Inner}, {});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::OrderAcrossUpdateRates) == std::vector<String>{"Test.Draw"});
        REQUIRE(HasText(result, "orders group Game.Inner (Fixed) before Test.Draw (Variable)"));
    }
}

TEST_CASE("Group nesting orders writers in sibling groups, not a child against its parent", "[ecs][schedule][groups]")
{
    SECTION("Inside Simulation, and in Physics")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.ZDeeper", .In = Deeper}, {{"Transform", W}});
        systems.Add(SystemInfo{.Name = "Test.APhysics", .In = Order::Physics}, {{"Transform", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.ZDeeper", "Test.APhysics"});
        REQUIRE(result.Value.Edges.empty());
    }

    SECTION("A child group and its parent do not order each other's systems")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.ZInner", .In = Inner}, {{"Transform", W}});
        systems.Add(SystemInfo{.Name = "Test.Sim", .In = Order::Simulation}, {{"Transform", W}});

        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::WriteConflict) == std::vector<String>{"Test.Sim", "Test.ZInner"});
    }
}

TEST_CASE("A cycle through groups is reported with its path", "[ecs][schedule][groups]")
{
    // LoopSecond runs after LoopFirst, and a member of LoopFirst runs after LoopSecond.
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.First", .In = LoopFirst, .After = LoopSecond}, {});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.Errors.size() == 1);
    const ScheduleError& error = result.Errors[0];
    REQUIRE(error.Kind == ScheduleErrorKind::Cycle);
    REQUIRE(error.Systems.size() == 1);
    REQUIRE(HasText(result,
                    "Test.First -> Game.LoopFirst (end) -> Game.LoopSecond (begin) -> Game.LoopSecond (end) -> "
                    "Test.First."));
    REQUIRE(HasText(result, "Game.LoopFirst (end) before Game.LoopSecond (begin) (group Game.LoopSecond has .After)"));
    REQUIRE(HasText(result, "Game.LoopSecond (end) before Test.First (.After)"));
}

TEST_CASE("SystemOf a system that was not added is an error", "[ecs][schedule][groups]")
{
    SECTION("In a system's After")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .After = SystemOf<FakeNotAdded>}, {});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::OrderTargetNotAdded) == std::vector<String>{"Test.A"});
        REQUIRE(HasText(result, "Test.A has .After = SystemOf<"));
        REQUIRE(HasText(result, "FakeNotAdded>, but no system of that type was added"));
    }

    SECTION("In a group's Before")
    {
        FakeSystems systems;
        systems.AddGroup(BeforeMissing);
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::OrderTargetNotAdded).empty());
        REQUIRE(HasText(result, "Group Game.BeforeMissing has .Before = SystemOf<"));
    }
}

TEST_CASE("A group is known by its name, not by its address", "[ecs][schedule][groups]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.InCopyA", .In = CopyA}, {{"X", W}});
    systems.Add(SystemInfo{.Name = "Test.AfterCopyB", .In = Order::Simulation, .After = CopyB}, {{"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.InCopyA", "Test.AfterCopyB"});
    REQUIRE(std::count_if(result.Value.Groups.begin(),
                          result.Value.Groups.end(),
                          [](const ScheduleGroup& group) { return StringView(group.Info->Name) == "Game.Copy"; }) == 1);
}

TEST_CASE("A FlushAfter group can be inside another group", "[ecs][schedule][groups]")
{
    SECTION("Ordered against every system")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Flushing", .In = FlushInner}, {});
        systems.Add(SystemInfo{.Name = "Test.Gameplay", .In = Order::Simulation, .After = FlushInner}, {});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {});

        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        const std::vector<ScheduleSegment>& segments = result.Value.GetSegments(UpdateRate::Fixed);
        REQUIRE(segments.size() == 2);
        REQUIRE(segments[0].FlushGroup == &FlushInner);
        REQUIRE(StageNames(result.Value) == Stages{{"Test.Flushing"}, {"Test.Gameplay"}, {"Test.Step"}});
    }

    SECTION("A system of the parent that nothing orders against the flush")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Flushing", .In = FlushInner}, {});
        systems.Add(SystemInfo{.Name = "Test.Gameplay", .In = Order::Simulation}, {});

        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::FlushNotOrdered) == std::vector<String>{"Test.Gameplay"});
    }
}

TEST_CASE("An order through a third system replaces the inferred edge", "[ecs][schedule][groups]")
{
    // A before B before C. C writes what A reads. The stated chain orders A and C, so there
    // is no data edge from C to A (one would make a loop).
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .Before = SystemOf<Tag::B>}, {{"X", R}});
    systems.Add<Tag::B>(SystemInfo{.Name = "Test.B", .In = Order::Simulation, .Before = SystemOf<Tag::C>}, {});
    systems.Add<Tag::C>(Sim("Test.C"), {{"X", W}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.A", "Test.B", "Test.C"});
    REQUIRE(result.Value.Lags.size() == 1);
    REQUIRE(String(result.Value.Systems[result.Value.Lags[0].Late]->Info.Name) == "Test.A");
}

// ---- System ids ------------------------------------------------------------------------------

namespace SystemTypeIdTest
{
    // EcsSystemTypeIdOtherFile.cpp declares Item with "struct". MSVC then names Box<Item>
    // "Box<struct Item>" there and "Box<class Item>" here.
    template <typename T>
    class Box
    {
    };

    class Item
    {
    };

    TypeHash BoxOfItemIdInOtherFile();
    StringView BoxOfItemNameInOtherFile();
} // namespace SystemTypeIdTest

TEST_CASE("A system id does not depend on class or struct inside template arguments", "[ecs][schedule][groups]")
{
    using SystemTypeIdTest::Box;
    using SystemTypeIdTest::Item;
    INFO("here: " << TypeNameOf<Box<Item>>() << ", other file: " << SystemTypeIdTest::BoxOfItemNameInOtherFile());
#if defined(_MSC_VER) && !defined(__clang__)
    // The raw names differ on MSVC; the id must not.
    REQUIRE(TypeNameOf<Box<Item>>() != SystemTypeIdTest::BoxOfItemNameInOtherFile());
#endif
    REQUIRE(SystemTypeId<Box<Item>>() == SystemTypeIdTest::BoxOfItemIdInOtherFile());

    using Detail::SystemTypeHash;
    STATIC_REQUIRE(SystemTypeHash("G::Box<struct G::Mis>") == SystemTypeHash("G::Box<class G::Mis>"));
    STATIC_REQUIRE(SystemTypeHash("G::Pair<struct G::A,enum G::B>") == SystemTypeHash("G::Pair<G::A,G::B>"));
    STATIC_REQUIRE(SystemTypeHash("G::Box<union G::U>") == SystemTypeHash("G::Box<G::U>"));
    // A plain name hashes as HashName does, and only a whole key is removed.
    STATIC_REQUIRE(SystemTypeHash("Fling::PhysicsStepSystem") == HashName("Fling::PhysicsStepSystem"));
    STATIC_REQUIRE(SystemTypeHash("G::Box<G::Subclass >") == HashName("G::Box<G::Subclass >"));
}

namespace AmbiguityTest
{
    /** EcsSystemTests.cpp: a system class named AmbiguousSystem in that file's anonymous namespace. */
    const SystemDesc& AmbiguousSystemInOtherFile();
} // namespace AmbiguityTest

namespace
{
    // The same qualified name as the class in EcsSystemTests.cpp, so the same SystemTypeId
    class AmbiguousSystem final : public System<AmbiguousSystem, Access<>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.AmbiguousHere", .In = Order::Simulation};

        void FixedUpdate(Context& /*ctx*/) const {}
    };
} // namespace

TEST_CASE("Two systems with one type id are an error", "[ecs][schedule][groups]")
{
    SECTION("Two descriptions with one type name")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.A"), {}, {.TypeName = "Game::Mover"});
        systems.Add(Sim("Test.B"), {}, {.TypeName = "Game::Mover"});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::AmbiguousSystemType) == std::vector<String>{"Test.A", "Test.B"});
        REQUIRE(HasText(result, "SystemOf<Game::Mover> matches both Test.A and Test.B."));
    }

    SECTION("Two classes in the anonymous namespaces of two files")
    {
        const SystemDesc& other = AmbiguityTest::AmbiguousSystemInOtherFile();
        REQUIRE(Detail::DescOf<AmbiguousSystem, AmbiguousSystem::Access>.TypeName == other.TypeName);
        FakeSystems systems;
        systems.Real<AmbiguousSystem>().Desc(other);
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::AmbiguousSystemType).size() == 2);
    }

    SECTION("Two instances of one class are a duplicate name only")
    {
        FakeSystems systems;
        systems.Real<AmbiguousSystem>().Real<AmbiguousSystem>();
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::DuplicateName).size() == 2);
    }
}

TEST_CASE("A system ordered against itself is a cycle", "[ecs][schedule][groups]")
{
    SECTION(".After = SystemOf<Self>")
    {
        FakeSystems systems;
        systems.Add<Tag::A>(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .After = SystemOf<Tag::A>}, {});
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::Cycle) == std::vector<String>{"Test.A"});
    }

    SECTION("RunBefore(x, x)")
    {
        FakeSystems systems;
        systems.Add<Tag::A>(Sim("Test.A"), {});
        systems.RunBefore(SystemOf<Tag::A>, SystemOf<Tag::A>);
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::Cycle) == std::vector<String>{"Test.A"});
    }
}

TEST_CASE("Error messages do not depend on the order systems are added in", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add(Sim("Test.A"), {{"X", W}});
    systems.Add(Sim("Test.B"), {{"X", W}, {"Y", W}});
    systems.Add(SystemInfo{.Name = "Test.C", .In = Order::Physics}, {{"Y", W}});
    systems.Add(SystemInfo{.Name = "Test.D", .In = Order::Physics}, {{"Y", W}});
    systems.Add(SystemInfo{.Name = "Test.Frame", .In = Order::Presentation}, {{"Z", W}});
    systems.Add(SystemInfo{.Name = "Test.Late", .In = Order::Simulation, .After = SystemOf<Tag::Nobody>}, {});

    const ScheduleResult forward = systems.Build(false);
    const ScheduleResult backward = systems.Build(true);
    REQUIRE(CountErrors(forward, ScheduleErrorKind::WriteConflict) == 2);
    REQUIRE(HasError(forward, ScheduleErrorKind::UpdateRateAccess));
    REQUIRE(HasError(forward, ScheduleErrorKind::OrderTargetNotAdded));
    REQUIRE(forward.ErrorReport() == backward.ErrorReport());
}

// ---- Resimulation membership -----------------------------------------------------------------

namespace
{
    static_assert(Detail::IsTickRoot(Order::Tick) && Detail::IsFrameRoot(Order::Frame));
    static_assert(!Detail::IsTickRoot(Order::Simulation) && !Detail::IsTickRoot(Order::Frame));

    // Groups in and out of the resimulation
    inline constexpr GroupInfo InSimulation{.Name = "Game.InSimulation", .In = Order::Simulation};
    inline constexpr GroupInfo DeepInPhysics{.Name = "Game.DeepInPhysics", .In = Order::Physics};
    inline constexpr GroupInfo DeeperInPhysics{.Name = "Game.DeeperInPhysics", .In = DeepInPhysics};
    inline constexpr GroupInfo ResimAgain{.Name = "Game.ResimAgain", .In = Order::Simulation, .Resim = true};
    inline constexpr GroupInfo ResimInFinalize{.Name = "Game.ResimInFinalize", .In = Order::Finalize, .Resim = true};
    inline constexpr GroupInfo ResimInFrame{.Name = "Game.ResimInFrame", .In = Order::Presentation, .Resim = true};
    inline constexpr GroupInfo ResimGroup{.Name = "Game.ResimGroup",
                                          .In = Order::Tick,
                                          .After = Order::Simulation,
                                          .Before = Order::Physics,
                                          .Resim = true};
    inline constexpr GroupInfo NotResimBetween{
        .Name = "Game.Between", .In = Order::Tick, .After = Order::Simulation, .Before = Order::Physics};
    inline constexpr GroupInfo NotResimFlush{.Name = "Game.NotResimFlush",
                                             .In = Order::Tick,
                                             .After = Order::Simulation,
                                             .Before = Order::Physics,
                                             .FlushAfter = true};
    inline constexpr GroupInfo LooseInTick{.Name = "Game.Loose", .In = Order::Tick};

    using Detail::IsResimPlacedRight;
    // Only a group directly in Tick sets Resim: setting it again inside Simulation is wrong too.
    static_assert(IsResimPlacedRight(Order::Simulation) && !IsResimPlacedRight(ResimAgain));
    static_assert(IsResimPlacedRight(ResimGroup) && IsResimPlacedRight(DeeperInPhysics));
    static_assert(!IsResimPlacedRight(ResimInFinalize) && !IsResimPlacedRight(ResimInFrame));
} // namespace

TEST_CASE("Resimulation membership comes from the group", "[ecs][schedule][resim]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Input", .In = Order::Input}, {{"Actions", W, Table, SimDomain, Rebuild}});
    systems.Add(SystemInfo{.Name = "Test.Gameplay", .In = Order::Simulation},
                {{"Actions", R, Table, SimDomain, Rebuild}, {"Mover", W}});
    systems.Add(SystemInfo{.Name = "Test.Nested", .In = InSimulation}, {{"Mover", R}, {"Speed", W}});
    systems.Add(SystemInfo{.Name = "Test.Step", .In = DeeperInPhysics}, {{"Speed", R}, {"Transform", W}});
    systems.Add(SystemInfo{.Name = "Test.Resolve", .In = Order::ResolveDeferred}, {{"Transform", R}, {"Hits", W}});
    systems.Add(SystemInfo{.Name = "Test.Finalize", .In = Order::Finalize},
                {{"Transform", R}, {"LocalToWorld", W, Table, SimDomain, Rebuild}});
    systems.Add(SystemInfo{.Name = "Test.Camera", .In = Order::Presentation},
                {{"Transform", R}, {"View", W, Table, Pres}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;

    // Per system, by the group it is in, at any depth
    std::vector<bool> resim;
    for (uint32 i = 0; i < schedule.Systems.size(); ++i)
    {
        resim.push_back(schedule.IsResim(i));
    }
    REQUIRE(resim == std::vector<bool>{false, true, true, true, true, false, false});
    REQUIRE(Names(schedule, schedule.ResimRunOrder()) ==
            std::vector<String>{"Test.Gameplay", "Test.Nested", "Test.Step", "Test.Resolve"});

    // Per group: the three engine groups that set Resim, and every group inside them
    std::vector<String> resimGroups;
    for (const ScheduleGroup& group : schedule.Groups)
    {
        if (group.Resim)
        {
            resimGroups.emplace_back(group.Info->Name);
        }
    }
    std::sort(resimGroups.begin(), resimGroups.end());
    REQUIRE(resimGroups == std::vector<String>{"Engine.Physics",
                                               "Engine.ResolveDeferred",
                                               "Engine.Simulation",
                                               "Game.DeepInPhysics",
                                               "Game.DeeperInPhysics",
                                               "Game.InSimulation"});

    // What a resimulation reads that only skipped systems write: a rollback restores or records it
    REQUIRE(schedule.ResimInputs.size() == 1);
    REQUIRE(String(schedule.ResimInputs[0].Name) == "Actions");
    REQUIRE(Names(schedule, schedule.ResimInputs[0].Writers) == std::vector<String>{"Test.Input"});
    REQUIRE(schedule.Dump().find("  resim reads from outside: Actions (from Test.Input)\n") != String::npos);
}

TEST_CASE("A resimulated writer of a type made outside the resimulation reads it from outside",
          "[ecs][schedule][resim]")
{
    // Write implies read: Test.Shoot may do aim.X += ..., so a replayed tick needs the Aim that
    // Test.Prep made in the live tick, which the resimulation does not make again
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "Test.Prep", .In = Order::Input}, {{"Aim", W, Table, SimDomain, Rebuild}});
    systems.Add(Sim("Test.Shoot"), {{"Aim", W, Table, SimDomain, Rebuild}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;
    REQUIRE(schedule.ResimInputs.size() == 1);
    REQUIRE(String(schedule.ResimInputs[0].Name) == "Aim");
    REQUIRE(Names(schedule, schedule.ResimInputs[0].Writers) == std::vector<String>{"Test.Prep"});
    REQUIRE_FALSE(schedule.ResimInputs[0].FromRunner);
}

TEST_CASE("Only a group directly in Tick sets Resim, and there is no opt-out", "[ecs][schedule][resim]")
{
    SECTION("A resim group inside a group that is not resimulated")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Late", .In = ResimInFinalize}, {{"X", W}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimPlacement).empty());
        REQUIRE(HasText(result, "Group Game.ResimInFinalize sets .Resim, but it is in Engine.Finalize."));
    }

    SECTION("A resim group inside a resimulated group")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Again", .In = ResimAgain}, {{"X", W}});
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::ResimPlacement).empty());
    }

    SECTION("A resim group at the frame rate")
    {
        FakeSystems systems;
        systems.AddGroup(ResimInFrame);
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::ResimPlacement).empty());
    }

    SECTION("A group in a resim group that does not set Resim is resimulated anyway")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Cosmetic", .In = InSimulation}, {{"X", W}});
        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(result.Value.IsResim(0));
    }

    SECTION("A game group directly in Tick can join")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Abilities", .In = ResimGroup}, {{"X", R}, {"Y", W}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", R}});
        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(result.Value.ResimRunOrder() == result.Value.RunOrder(UpdateRate::Fixed));
    }
}

TEST_CASE("A fixed system outside the resimulation cannot make what the resimulation needs", "[ecs][schedule][resim]")
{
    SECTION("It writes state that a rollback restores")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Input", .In = Order::Input}, {{"Score", W, Single}});
        systems.Add(Sim("Test.Gameplay"), {{"Score", R, Single}, {"X", W}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimSkipsState) == std::vector<String>{"Test.Input"});
        REQUIRE(HasText(result,
                        "Test.Input (group Engine.Input) writes Score, which a rollback restores, but a "
                        "resimulation does not run it"));
    }

    SECTION("After the resimulated part, it writes what a resimulated system reads")
    {
        // Finalize makes Rebuild data. A resimulated reader would see the value of the last live tick.
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"World", R, Table, SimDomain, Rebuild}, {"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Finish", .In = Order::Finalize},
                    {{"X", R}, {"World", W, Table, SimDomain, Rebuild}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimSkipsState) ==
                std::vector<String>{"Test.Finish", "Test.Gameplay"});
        REQUIRE(HasText(result, "Test.Finish (group Engine.Finalize) writes World, which Test.Gameplay reads"));
    }

    SECTION("After the resimulated part, it writes what a resimulated system writes, which reads it too")
    {
        // Write implies read: Test.Aim may do aim.X += ..., so it would start from the last live tick
        FakeSystems systems;
        systems.Add(Sim("Test.Aim"), {{"Aim", W, Table, SimDomain, Rebuild}});
        systems.Add(SystemInfo{.Name = "Test.Finish", .In = Order::Finalize}, {{"Aim", W, Table, SimDomain, Rebuild}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimSkipsState) ==
                std::vector<String>{"Test.Finish", "Test.Aim"});
        REQUIRE(HasText(result, "Test.Finish (group Engine.Finalize) writes Aim, which Test.Aim reads"));
    }

    SECTION("Finalize and a look camera are fine")
    {
        // The runner writes InputState before the resimulation; Engine.Transform writes
        // LocalToWorld (Rebuild) and TransformWork (Keep) after it, and no resimulated system
        // reads them
        FakeSystems systems;
        systems.Real<TransformSystem>().Real<TestLook::CameraSystem>();
        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(CountErrors(result, ScheduleErrorKind::ResimSkipsState) == 0);
    }
}

TEST_CASE("A resimulated system appends only to a queue that the resimulation consumes", "[ecs][schedule][resim]")
{
    SECTION("Requests to the application, which the world takes after the tick")
    {
        // Sandbox.AppKeys in Order::Simulation: a replayed tick would ask to quit again
        FakeSystems systems;
        systems.Add(Sim("Test.Keys"), {{"AppRequests", A, Queue}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimAppendsOutput) == std::vector<String>{"Test.Keys"});
        REQUIRE(HasText(result,
                        "Test.Keys (group Engine.Simulation) appends to AppRequests, but no system consumes "
                        "it: code outside the systems takes it after the tick"));
    }

    SECTION("A queue that a system outside the resimulation consumes")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.Effects"), {{"Sounds", A, Queue}});
        systems.Add(SystemInfo{.Name = "Test.Play", .In = Order::Finalize}, {{"Sounds", C, Queue}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimAppendsOutput) ==
                std::vector<String>{"Test.Effects", "Test.Play"});
        REQUIRE(HasText(result, "appends to Sounds, but Test.Play consumes it outside the resimulation"));
    }

    SECTION("Consumers in two worlds: the one in the appender's world decides, in any add order")
    {
        for (const bool reversed : {false, true})
        {
            FakeSystems systems;
            systems.Add(SystemInfo{.Name = "Test.Effects", .In = Order::Simulation}, {{"Sounds", A, Queue}});
            systems.Add(SystemInfo{.Name = "Test.EditPlay", .In = Order::Finalize, .RunIn = WorldRole::Edit},
                        {{"Sounds", C, Queue}});
            systems.Add(SystemInfo{.Name = "Test.Play", .In = Order::ResolveDeferred}, {{"Sounds", C, Queue}});
            REQUIRE(systems.Build(reversed).ErrorReport() == "");
        }
        FakeSystems outside;
        outside.Add(SystemInfo{.Name = "Test.Effects", .In = Order::Simulation}, {{"Sounds", A, Queue}});
        outside.Add(SystemInfo{.Name = "Test.EditPlay", .In = Order::ResolveDeferred, .RunIn = WorldRole::Edit},
                    {{"Sounds", C, Queue}});
        outside.Add(SystemInfo{.Name = "Test.Play", .In = Order::Finalize}, {{"Sounds", C, Queue}});
        REQUIRE(OnlyError(outside.Build(), ScheduleErrorKind::ResimAppendsOutput) ==
                std::vector<String>{"Test.Effects", "Test.Play"});
    }

    SECTION("In Order::Input, or with a resimulated consumer, it is fine")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Keys", .In = Order::Input}, {{"AppRequests", A, Queue}});
        systems.Add(Sim("Test.Spawner"), {{"Spawns", A, Queue}});
        systems.Add(SystemInfo{.Name = "Test.Spawn", .In = Order::ResolveDeferred}, {{"Spawns", C, Queue}});
        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
    }
}

TEST_CASE("A system that is not resimulated cannot run between two that are", "[ecs][schedule][resim]")
{
    SECTION("Placed there by group order")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Between", .In = NotResimBetween}, {{"Z", W, Table, SimDomain, Rebuild}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", W}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimGap) ==
                std::vector<String>{"Test.Between", "Test.Gameplay", "Test.Step"});
        REQUIRE(HasText(result,
                        "Test.Between (group Game.Between) is not resimulated, but it does not run before "
                        "Test.Gameplay or after Test.Step, which are."));
    }

    SECTION("Placed there by its data: a loose group, ordered only by what it reads and writes")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Loose", .In = LooseInTick},
                    {{"X", R}, {"Y", W, Table, SimDomain, Rebuild}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", R, Table, SimDomain, Rebuild}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimGap) ==
                std::vector<String>{"Test.Loose", "Test.Gameplay", "Test.Step"});
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Gameplay", "Test.Loose", "Test.Step"});
    }

    SECTION("A loose group that nothing orders: the same error under any name")
    {
        // In run order, a system with no edges sorts by name: before Simulation as "Test.AStats",
        // between Simulation and Physics as "Test.ZStats". The rule is on the graph, so both fail.
        for (const char* name : {"Test.AStats", "Test.ZStats"})
        {
            INFO(name);
            FakeSystems systems;
            systems.Add(Sim("Test.Gameplay"), {{"X", W}});
            systems.Add(SystemInfo{.Name = name, .In = LooseInTick}, {{"Stats", W, Table, SimDomain, Rebuild}});
            systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", W}});
            const ScheduleResult result = systems.Build();
            REQUIRE(OnlyError(result, ScheduleErrorKind::ResimGap) == std::vector<String>{name, "Test.Gameplay"});
        }
    }

    SECTION("A system that only reads from the resimulation, with no order against all of it")
    {
        // Its data puts it after Test.Gameplay, but nothing orders it against Test.Step
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Reader", .In = LooseInTick}, {{"X", R}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", W}});
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::ResimGap) ==
                std::vector<String>{"Test.Reader", "Test.Gameplay", "Test.Step"});
    }

    SECTION("A FlushAfter group that is not resimulated")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Flushing", .In = NotResimFlush}, {{"Z", W, Table, SimDomain, Rebuild}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", W}});
        const ScheduleResult result = systems.Build();
        REQUIRE(OnlyError(result, ScheduleErrorKind::ResimGap) ==
                std::vector<String>{"Test.Flushing", "Test.Gameplay", "Test.Step"});
    }

    SECTION("Before or after all of them is fine")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "Test.Input", .In = Order::Input}, {{"X", W, Table, SimDomain, Rebuild}});
        systems.Add(Sim("Test.Gameplay"), {{"X", R, Table, SimDomain, Rebuild}, {"Y", W}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", W}});
        systems.Add(SystemInfo{.Name = "Test.Late", .In = LateGroup}, {{"Y", R}});
        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(RunNames(result.Value) == std::vector<String>{"Test.Input", "Test.Gameplay", "Test.Step", "Test.Late"});
        REQUIRE(result.Value.ResimRunOrder().size() == 2);
    }

    SECTION("The same group with .Resim is part of the resimulation")
    {
        FakeSystems systems;
        systems.Add(Sim("Test.Gameplay"), {{"X", W}});
        systems.Add(SystemInfo{.Name = "Test.Abilities", .In = ResimGroup}, {{"Z", W}});
        systems.Add(SystemInfo{.Name = "Test.Step", .In = Order::Physics}, {{"Y", W}});
        REQUIRE(systems.Build().ErrorReport() == "");
    }
}

namespace HintTest
{
    class MoverSystem;
    class RotatorSystem;

    template <typename T>
    struct Box;
} // namespace HintTest

TEST_CASE("The write conflict fix names the class of the system", "[ecs][schedule]")
{
    FakeSystems systems;
    systems.Add<HintTest::RotatorSystem>(Sim("Test.Rotator"), {{"Transform", W}});
    systems.Add<HintTest::MoverSystem>(Sim("Test.Mover"), {{"Transform", W}});
    const ScheduleResult result = systems.Build();
    REQUIRE(OnlyError(result, ScheduleErrorKind::WriteConflict).size() == 2);
    const String& message = result.Errors[0].Message;
    INFO(message);
    REQUIRE(message.find(".After = Fling::SystemOf<HintTest::MoverSystem> to Test.Rotator's SystemInfo") !=
            String::npos);

    // In a template argument too, with no "class " or "struct " from the compiler's name
    FakeSystems templated;
    templated.Add<HintTest::Box<HintTest::MoverSystem>>(Sim("Test.BoxMover"), {{"Transform", W}});
    templated.Add(Sim("Test.Other"), {{"Transform", W}});
    const ScheduleResult boxed = templated.Build();
    REQUIRE(OnlyError(boxed, ScheduleErrorKind::WriteConflict).size() == 2);
    INFO(boxed.Errors[0].Message);
    REQUIRE(boxed.Errors[0].Message.find(".After = Fling::SystemOf<HintTest::Box<HintTest::MoverSystem>> to "
                                         "Test.Other") != String::npos);
    REQUIRE(boxed.Errors[0].Message.find("class ") == String::npos);
    REQUIRE(boxed.Errors[0].Message.find("struct ") == String::npos);

    // A SystemOf that was not added is named the same way
    FakeSystems missing;
    missing.Add(SystemInfo{.Name = "Test.A", .In = Order::Simulation, .After = SystemOf<HintTest::RotatorSystem>}, {});
    REQUIRE(HasText(missing.Build(), ".After = SystemOf<HintTest::RotatorSystem>, but no system of that type"));
}

// ---- What crosses from the game side to the frame side ---------------------------------

namespace
{
    /** The names of the published types. */
    std::vector<String> PublishedNames(const Schedule& schedule)
    {
        std::vector<String> names;
        for (const PublishedType& type : schedule.Published)
        {
            names.emplace_back(type.Name);
        }
        return names;
    }

    const PublishedType& PublishedNamed(const Schedule& schedule, const StringView name)
    {
        const auto it = std::find_if(schedule.Published.begin(),
                                     schedule.Published.end(),
                                     [name](const PublishedType& type) { return StringView(type.Name) == name; });
        REQUIRE(it != schedule.Published.end());
        return *it;
    }
} // namespace

TEST_CASE("The published set is the Simulation data that a variable rate system reads", "[ecs][schedule][published]")
{
    FakeSystems systems;
    systems.Add(SystemInfo{.Name = "T.Input", .In = Order::Input},
                {{"Input", W, Single, SimDomain, Rebuild}, {"Cut", W, Single, SimDomain, Rebuild}});
    systems.Add(Sim("T.Move"), {{"Input", R, Single, SimDomain, Rebuild}, {"Pos", W}, {"Score", W, Single}});
    systems.Add(SystemInfo{.Name = "T.World", .In = Order::Finalize},
                {{"Pos", R}, {"World", W, Table, SimDomain, Rebuild}});
    systems.Add(SystemInfo{.Name = "F.Smooth", .In = Order::Presentation},
                {{"Pos", R}, {"Cut", R, Single}, {"View", W, Table, Pres}});
    systems.Add(SystemInfo{.Name = "F.Draw", .In = Order::Render},
                {{"World", R}, {"View", R, Table, Pres}, {"Score", R, Single}, {"Light", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;

    // Complete: every Simulation type a frame system reads, written or not (Light changes only by load).
    // Minimal: not Input (only the tick reads it), not View (a Presentation component). Sorted by name.
    REQUIRE(PublishedNames(schedule) == std::vector<String>{"Cut", "Light", "Pos", "Score", "World"});
    CHECK(Names(schedule, PublishedNamed(schedule, "Pos").Readers) == std::vector<String>{"F.Smooth"});
    CHECK(Names(schedule, PublishedNamed(schedule, "World").Readers) == std::vector<String>{"F.Draw"});
    CHECK(PublishedNamed(schedule, "Score").Kind == Single);
    CHECK(PublishedNamed(schedule, "Light").Hash == HashName("Light"));
    CHECK(schedule.Dump().find("Published each tick, for Order::Frame\n  Cut (singleton), read by F.Smooth\n") !=
          String::npos);
}

namespace
{
    /** A frame system that blends Transform between the two blend states, as the renderer's snapshot does. */
    class BlendProbeSystem final
        : public System<
              BlendProbeSystem,
              Access<Read<RealFrameTime>, Read<Transform>, ReadBlend<Transform>, Read<LocalToWorld>, Read<Parent>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.BlendProbe", .In = Order::Render};

        void Update(Context&) const {}
    };
} // namespace

TEST_CASE("ReadBlend marks its Read row and adds none", "[ecs][schedule][published][interp]")
{
    constexpr auto& table = BlendProbeSystem::Access::Table;
    STATIC_REQUIRE(table.size() == 4);
    STATIC_REQUIRE(BlendProbeSystem::Access::HasBlend);
    STATIC_REQUIRE(AllowsBlend<BlendProbeSystem::Access, Transform>);
    STATIC_REQUIRE(AllowsBlend<BlendProbeSystem::Access, const Transform>);
    STATIC_REQUIRE_FALSE(AllowsBlend<BlendProbeSystem::Access, LocalToWorld>);
    for (const AccessEntry& entry : table)
    {
        CHECK(entry.Mode == AccessMode::Read);
        CHECK(entry.Blend == (entry.Hash == TypeHashOf<Transform>));
    }
}

TEST_CASE("The published set holds for a blend exactly the types a frame system declares",
          "[ecs][schedule][published][interp]")
{
    FakeSystems systems;
    systems.Real<TransformSystem>();
    systems.Real<TestLook::CameraSystem>().Real<TestLook::CameraViewSystem>();
    systems.Real<BlendProbeSystem>();
    // A second reader of Transform that does not blend
    systems.Add(SystemInfo{.Name = "Test.Snapshot", .In = Order::Render}, {{"Transform", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;
    REQUIRE(PublishedNames(schedule) == std::vector<String>{"LocalToWorld", "Parent", "TestCamera", "Transform"});

    // Only Transform, and only for the system that declared it. The look camera is shown from the
    // latest tick and the pending input, never blended, so it is not held.
    for (const PublishedType& type : schedule.Published)
    {
        INFO(type.Name);
        CHECK(type.IsBlended() == (StringView(type.Name) == "Transform"));
    }
    CHECK(Names(schedule, PublishedNamed(schedule, "Transform").Readers) ==
          std::vector<String>{"Test.BlendProbe", "Test.Snapshot"});
    CHECK(Names(schedule, PublishedNamed(schedule, "Transform").BlendReaders) ==
          std::vector<String>{"Test.BlendProbe"});

    // The dump shows it, on the published type's line
    const String dump = schedule.Dump();
    CHECK(dump.find("  Transform (table), read by Test.BlendProbe, Test.Snapshot; held for the blend of "
                    "Test.BlendProbe\n") != String::npos);
    CHECK(dump.find("  TestCamera (table), read by Test.CameraView\n") != String::npos);
}

TEST_CASE("The engine systems and a look camera publish what the frame reads", "[ecs][schedule][published]")
{
    FakeSystems systems;
    systems.Real<TransformSystem>();
    systems.Real<TestLook::CameraSystem>().Real<TestLook::CameraViewSystem>();
    // Stands in for the renderer's snapshot, which EngineTests cannot compile
    systems.Add(SystemInfo{.Name = "Test.Snapshot", .In = Order::Render}, {{"Transform", R}, {"LocalToWorld", R}});

    const ScheduleResult result = systems.Build();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;
    // The frame side's input cut comes with the packet's TickStamp (RealFrameTime), not as a component
    REQUIRE(PublishedNames(schedule) == std::vector<String>{"LocalToWorld", "TestCamera", "Transform"});
    CHECK(Names(schedule, PublishedNamed(schedule, "TestCamera").Readers) == std::vector<String>{"Test.CameraView"});
    CHECK(PublishedNamed(schedule, "TestCamera").Hash == TypeHashOf<TestLook::TestCamera>);

    // The look camera reads this tick's input, which the runner writes and the resimulation does not make
    REQUIRE(schedule.ResimInputs.size() == 1);
    CHECK(schedule.ResimInputs[0].Hash == TypeHashOf<InputState>);
    CHECK(schedule.ResimInputs[0].FromRunner);
    CHECK(schedule.ResimInputs[0].Writers.empty());
    const String dump = schedule.Dump();
    CHECK(dump.find("  resim reads from outside: InputState (from the runner)\n") != String::npos);
    CHECK(dump.find("Fixed (once per tick)\n  the runner writes first: SimTime, InputState\n") != String::npos);
    CHECK(dump.find("Variable (once per frame)\n  the runner writes first: RealFrameTime, PendingInput\n") !=
          String::npos);
    // No system writes the input singletons, so their readers have no data edge for them
    for (const ScheduleEdge& edge : schedule.Edges)
    {
        const String type = edge.TypeName != nullptr ? String(edge.TypeName) : String();
        CHECK(type != "InputState");
        CHECK(type != "PendingInput");
    }
}

TEST_CASE("The schedule does not depend on the order systems are added in", "[ecs][schedule][published]")
{
    FakeSystems systems;
    systems.Real<TransformSystem>();
    systems.Real<TestLook::CameraSystem>().Real<TestLook::CameraViewSystem>();
    systems.Add(SystemInfo{.Name = "Test.Snapshot", .In = Order::Render}, {{"Transform", R}, {"LocalToWorld", R}});
    systems.Add(Sim("Test.Mover"), {{"InputState", R, Single}, {"Transform", W}, {"Velocity", R}});
    systems.Add(SystemInfo{.Name = "Test.Push", .In = Order::Physics}, {{"Velocity", W}, {"Transform", R}});

    const ScheduleResult forward = systems.Build(false);
    const ScheduleResult backward = systems.Build(true);
    REQUIRE(forward.ErrorReport() == "");
    REQUIRE(backward.ErrorReport() == "");
    // The dump prints every list of the schedule, by name
    INFO(forward.Value.Dump());
    CHECK(forward.Value.Dump() == backward.Value.Dump());
}

TEST_CASE("A hand-made description that breaks an update rate rule is an error", "[ecs][schedule][published]")
{
    SECTION("A variable rate system writes, appends to, or consumes a Simulation component")
    {
        for (const AccessMode mode : {W, A, C})
        {
            FakeSystems systems;
            systems.Add(SystemInfo{.Name = "F.Aim", .In = Order::Presentation}, {{"Aim", mode}});
            const ScheduleResult result = systems.Build();
            REQUIRE(OnlyError(result, ScheduleErrorKind::UpdateRateAccess) == std::vector<String>{"F.Aim"});
            REQUIRE(HasText(result,
                            "F.Aim (group Engine.Presentation, variable rate) writes, appends to, or consumes Aim, a "
                            "Simulation component."));
        }
    }

    SECTION("A fixed rate system reads a Presentation component")
    {
        FakeSystems systems;
        systems.Add(Sim("T.Shoot"), {{"Theme", R, Single, Pres}});
        REQUIRE(OnlyError(systems.Build(), ScheduleErrorKind::UpdateRateAccess) == std::vector<String>{"T.Shoot"});
    }

    SECTION("A variable rate system spawns or uses RawStorage")
    {
        FakeSystems systems;
        systems.Add(SystemInfo{.Name = "F.Spawner", .In = Order::Presentation}, {}, {.Spawns = true});
        systems.Add(SystemInfo{.Name = "F.Raw", .In = Order::Render}, {}, {.RawStorage = true});
        systems.Add(Sim("T.Spawner"), {}, {.Spawns = true, .RawStorage = true});
        const ScheduleResult result = systems.Build();
        REQUIRE(CountErrors(result, ScheduleErrorKind::UpdateRateAccess) == 2);
        REQUIRE(result.Errors.size() == 2);
    }

    SECTION("The frame reads what the tick writes: published, no error")
    {
        FakeSystems systems;
        systems.Add(Sim("T.Aim"), {{"Aim", W}});
        systems.Add(SystemInfo{.Name = "F.Aim", .In = Order::Presentation}, {{"Aim", R}});
        const ScheduleResult result = systems.Build();
        REQUIRE(result.ErrorReport() == "");
        REQUIRE(PublishedNames(result.Value) == std::vector<String>{"Aim"});
    }
}
