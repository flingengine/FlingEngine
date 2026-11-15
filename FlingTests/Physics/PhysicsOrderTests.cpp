#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "Physics.h"
#include "PhysicsSystemsFwd.h"
#include "Strings/String.h"
#include "Systems/PhysicsStepSystem.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

// The order of the engine and physics systems, with the real PhysicsStepSystem.

using namespace Fling;

namespace OrderExample
{
    // What a game's order header holds: its group, and forward declarations of its systems.
    class SuspensionSystem;
    class SteeringSystem;
    class HoverSystem;

    /** Vehicle work runs in the engine's physics group, before the physics step. */
    inline constexpr GroupInfo Vehicles{
        .Name = "Game.Vehicles", .In = Order::Physics, .Before = SystemOf<PhysicsStepSystem>};

    struct VehicleSuspension
    {
        static constexpr ComponentInfo Component{.Name = "Test.VehicleSuspension"};
        float Compression = 0.0f;
    };

    struct VehicleSteering
    {
        static constexpr ComponentInfo Component{.Name = "Test.VehicleSteering"};
        float Angle = 0.0f;
    };

    // The names sort after "Physics.Step", so only the group order can put the step last.
    class SuspensionSystem final : public System<SuspensionSystem, Access<Write<VehicleSuspension>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Vehicles.Suspension", .In = Vehicles};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    class SteeringSystem final : public System<SteeringSystem, Access<Write<VehicleSteering>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Vehicles.Steering", .In = Vehicles};

        void FixedUpdate(Context& /*ctx*/) const {}
    };

    /** Writes Transform, as PhysicsStepSystem does. */
    class HoverSystem final
        : public System<HoverSystem,
                        Access<Read<SimTime>, Read<VehicleSuspension>, Read<VehicleSteering>, Write<Transform>>>
    {
    public:
        static constexpr SystemInfo Info{
            .Name = "Vehicles.Hover", .In = Vehicles, .After = {SystemOf<SuspensionSystem>, SystemOf<SteeringSystem>}};

        void FixedUpdate(Context& /*ctx*/) const {}
    };
} // namespace OrderExample

namespace
{
    /** Stands in for the renderer's snapshot, which PhysicsTests cannot compile: a frame system that reads poses. */
    class SnapshotSystem final : public System<SnapshotSystem, Access<Read<Transform>, Read<LocalToWorld>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Test.Snapshot", .In = Order::Render};

        void Update(Context& /*ctx*/) const {}
    };

    std::vector<String> Names(const Schedule& schedule, const std::vector<uint32>& systems)
    {
        std::vector<String> names;
        for (const uint32 i : systems)
        {
            names.emplace_back(schedule.Systems[i]->Info.Name);
        }
        return names;
    }

    std::vector<String> FixedRunNames(const Schedule& schedule)
    {
        return Names(schedule, schedule.RunOrder(UpdateRate::Fixed));
    }

    String ReadText(const std::filesystem::path& path)
    {
        std::ifstream file(path, std::ios::binary);
        std::stringstream text;
        text << file.rdbuf();
        String out = text.str();
        std::erase(out, '\r');
        return out;
    }

    /** The first line where two texts differ, as "line N: expected ... / actual ...". */
    String FirstDifference(const StringView expected, const StringView actual)
    {
        std::istringstream a{String(expected)};
        std::istringstream b{String(actual)};
        String lineA;
        String lineB;
        for (int line = 1;; ++line)
        {
            const bool hasA = static_cast<bool>(std::getline(a, lineA));
            const bool hasB = static_cast<bool>(std::getline(b, lineB));
            if (!hasA && !hasB)
            {
                return "no difference";
            }
            if (!hasA || !hasB || lineA != lineB)
            {
                return "line " + std::to_string(line) + ":\n  expected: " + (hasA ? lineA : "(end of file)") +
                       "\n  actual:   " + (hasB ? lineB : "(end of file)");
            }
        }
    }
} // namespace

TEST_CASE("A game group runs inside Physics, before the physics step", "[ecs][schedule][physics]")
{
    SystemRegistry registry;
    registry.Add<PhysicsStepSystem>();
    registry.Add<OrderExample::HoverSystem>();
    registry.Add<OrderExample::SteeringSystem>();
    registry.Add<OrderExample::SuspensionSystem>();

    const ScheduleResult result = registry.BuildSchedule();
    // Hover and the step both write Transform. The group order puts Hover first, so it is not a conflict.
    REQUIRE(result.ErrorReport() == "");
    REQUIRE(FixedRunNames(result.Value) ==
            std::vector<String>{"Vehicles.Steering", "Vehicles.Suspension", "Vehicles.Hover", "Physics.Step"});

    // No data edge goes into the step: the group order is the only reason it is last.
    for (const ScheduleEdge& edge : result.Value.Edges)
    {
        REQUIRE(String(result.Value.Systems[edge.To]->Info.Name) != "Physics.Step");
        REQUIRE(edge.Reason == EdgeReason::After);
    }
    REQUIRE(result.Value.Edges.size() == 2);
    REQUIRE(result.Value.Dump().find("[Game.Vehicles]") != String::npos);
}

TEST_CASE("The engine and physics schedule matches the checked-in file", "[ecs][schedule][physics]")
{
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterEngine(modules);
    RegisterPhysics(modules);

    const ScheduleResult result = systems.BuildSchedule();
    REQUIRE(result.ErrorReport() == "");

    const std::filesystem::path expectedPath =
        std::filesystem::path(__FILE__).parent_path() / "Schedules" / "EnginePhysics.schedule.txt";
    const String actual = result.Value.Dump();
    const String expected = ReadText(expectedPath);
    if (actual != expected)
    {
        const std::filesystem::path actualPath = std::filesystem::absolute("EnginePhysics.schedule.actual.txt");
        std::ofstream(actualPath, std::ios::binary) << actual;
        FAIL("The engine and physics schedule changed. If the change is wanted, copy\n  "
             << actualPath.string() << "\nover\n  " << expectedPath.string()
             << "\nand commit it, so that review shows the new order. First difference, "
             << FirstDifference(expected, actual));
    }
}

TEST_CASE("The engine and physics systems publish what the frame reads", "[ecs][schedule][physics]")
{
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterEngine(modules);
    RegisterPhysics(modules);
    systems.Add<SnapshotSystem>();

    const ScheduleResult result = systems.BuildSchedule();
    REQUIRE(result.ErrorReport() == "");
    const Schedule& schedule = result.Value;

    std::vector<String> published;
    for (const PublishedType& type : schedule.Published)
    {
        published.emplace_back(type.Name);
    }
    // The frame side's input cut comes with the packet's TickStamp, not as a published component
    REQUIRE(published == std::vector<String>{"LocalToWorld", "Transform"});
    REQUIRE(Names(schedule, schedule.Published[1].Readers) == std::vector<String>{"Test.Snapshot"});
    REQUIRE(schedule.Published[1].Hash == TypeHashOf<Transform>);

    // The physics step makes again in a resimulation everything that it reads, except the
    // tick's time, which the runner writes before each tick
    REQUIRE(schedule.ResimInputs.size() == 1);
    REQUIRE(schedule.ResimInputs[0].Hash == TypeHashOf<SimTime>);
    REQUIRE(schedule.ResimInputs[0].FromRunner);
    REQUIRE(schedule.ResimInputs[0].Writers.empty());
}
