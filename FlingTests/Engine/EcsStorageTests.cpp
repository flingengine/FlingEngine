#include "Ecs/EntityStorage.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <memory>
#include <string>
#include <vector>

using namespace Fling;

namespace
{
    struct Pos
    {
        static constexpr ComponentInfo Component{.Name = "Storage.Pos"};
        float X = 0.0f;
    };

    struct Vel
    {
        static constexpr ComponentInfo Component{.Name = "Storage.Vel"};
        float X = 0.0f;
    };

    struct Label
    {
        static constexpr ComponentInfo Component{.Name = "Storage.Label"};
        std::string Text;
    };

    struct Marked
    {
        static constexpr ComponentInfo Component{.Name = "Storage.Marked", .Kind = ComponentKind::Tag};
    };

    struct Burning
    {
        static constexpr ComponentInfo Component{.Name = "Storage.Burning", .Storage = ComponentStorage::Sparse};
        int Ticks = 0;
    };

    struct Visible
    {
        static constexpr ComponentInfo Component{.Name = "Storage.Visible", .Storage = ComponentStorage::Enableable};
        int Layer = 0;
    };

    // Five components that five systems add to one entity in one tick
    template <int N>
    struct Part
    {
        static constexpr ComponentInfo Component{.Name = N == 0   ? "Storage.Part0"
                                                         : N == 1 ? "Storage.Part1"
                                                         : N == 2 ? "Storage.Part2"
                                                         : N == 3 ? "Storage.Part3"
                                                                  : "Storage.Part4"};
        int Value = N;
    };

    template <int N>
    class AddPartSystem final : public System<AddPartSystem<N>, Access<Read<Pos>, Write<Part<N>>>>
    {
    public:
        using Base = System<AddPartSystem<N>, Access<Read<Pos>, Write<Part<N>>>>;
        static constexpr SystemInfo Info{.Name = N == 0   ? "Storage.AddPart0"
                                                 : N == 1 ? "Storage.AddPart1"
                                                 : N == 2 ? "Storage.AddPart2"
                                                 : N == 3 ? "Storage.AddPart3"
                                                          : "Storage.AddPart4",
                                         .Phase = Phases::Simulation};

        void FixedUpdate(typename Base::Context& ctx) const
        {
            for (auto [entity, pos] : ctx.template Query<const Pos>())
            {
                (void)pos;
                ctx.Add(entity, Part<N>{N * 10});
            }
        }
    };

    /** Spawns one entity with a Pos for each entity with a Pos it sees. */
    class SpawnWhileIteratingSystem final : public System<SpawnWhileIteratingSystem, Access<Write<Pos>, Spawns>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Storage.SpawnWhileIterating", .Phase = Phases::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            int seen = 0;
            for (auto [entity, pos] : ctx.Query<Pos>())
            {
                (void)entity;
                pos.X += 1.0f;
                ++seen;
                ctx.Spawn(Pos{-1.0f});
            }
            (void)seen;
        }
    };

    std::unique_ptr<SystemRunner> StartRunner(EntityStorage& storage, SystemRegistry& systems)
    {
        ScheduleResult schedule = systems.BuildSchedule();
        REQUIRE(schedule.ErrorReport() == "");
        auto runner = std::make_unique<SystemRunner>(systems, std::move(schedule.Value), storage);
        runner->Create();
        return runner;
    }
} // namespace

TEST_CASE("Adding and removing components moves the entity between archetypes", "[ecs][storage]")
{
    EntityStorage storage;
    const Entity a = storage.Create();
    const Entity b = storage.Create();
    storage.Add<Pos>(a, Pos{1.0f});
    storage.Add<Label>(a, Label{"a"});
    storage.Add<Pos>(b, Pos{2.0f});
    storage.Add<Vel>(b, Vel{3.0f});
    storage.Add<Label>(b, Label{"b"});

    // Values survive the moves, including a type with heap data
    REQUIRE(storage.Get<Pos>(a).X == 1.0f);
    REQUIRE(storage.Get<Label>(b).Text == "b");

    storage.Remove<Pos>(a);
    REQUIRE_FALSE(storage.Has<Pos>(a));
    REQUIRE(storage.Get<Label>(a).Text == "a");
    REQUIRE(storage.Count<Pos>() == 1);
    REQUIRE(storage.Count<Label>() == 2);

    // Destroying a removes its row. b keeps its values.
    storage.Destroy(a);
    REQUIRE(storage.Get<Pos>(b).X == 2.0f);
    REQUIRE(storage.Get<Vel>(b).X == 3.0f);
    REQUIRE(storage.Get<Label>(b).Text == "b");
    REQUIRE(storage.EntityCount() == 1);
}

TEST_CASE("Rows stay right when many entities come and go", "[ecs][storage]")
{
    EntityStorage storage;
    std::vector<Entity> entities;
    for (int i = 0; i < 2000; ++i)
    {
        const Entity e = storage.Create();
        storage.Add<Pos>(e, Pos{static_cast<float>(i)});
        if (i % 3 == 0)
        {
            storage.Add<Vel>(e, Vel{static_cast<float>(-i)});
        }
        entities.push_back(e);
    }
    // Remove every other entity, which fills holes from the ends of the chunks
    for (size_t i = 0; i < entities.size(); i += 2)
    {
        storage.Destroy(entities[i]);
    }
    for (size_t i = 1; i < entities.size(); i += 2)
    {
        REQUIRE(storage.Get<Pos>(entities[i]).X == static_cast<float>(i));
        if (i % 3 == 0)
        {
            REQUIRE(storage.Get<Vel>(entities[i]).X == -static_cast<float>(i));
        }
    }
    REQUIRE(storage.Count<Pos>() == 1000);
    REQUIRE(storage.View<Pos, Vel>().Size() == storage.Count<Vel>());
    // Several chunks, because a chunk holds at most 16 KiB
    REQUIRE(storage.Stats().Chunks > 2);
}

TEST_CASE("Tags, Sparse, and Enableable components", "[ecs][storage]")
{
    EntityStorage storage;
    const Entity e = storage.Create();
    storage.Add<Pos>(e, Pos{5.0f});
    storage.Add<Marked>(e);
    REQUIRE(storage.Has<Marked>(e));
    REQUIRE(storage.View<Pos, Marked>().Size() == 1);

    SECTION("Sparse add and remove do not move the entity or make archetypes")
    {
        const EcsStats before = storage.Stats();
        Burning& burning = storage.Add<Burning>(e, Burning{3});
        Burning* address = &burning;
        storage.Add<Vel>(e);
        // A sparse value has a stable address, even when the entity moves
        REQUIRE(storage.TryGet<Burning>(e) == address);
        storage.Remove<Vel>(e);
        storage.Remove<Burning>(e);
        REQUIRE_FALSE(storage.Has<Burning>(e));
        const EcsStats after = storage.Stats();
        // Only the Vel add and remove moved it
        REQUIRE(after.StructuralMoves == before.StructuralMoves + 2);
        REQUIRE(storage.View<Burning>().Empty());
    }

    SECTION("An Enableable component turns off without a move, and views skip it")
    {
        storage.Add<Visible>(e, Visible{2});
        const EcsStats before = storage.Stats();
        storage.SetEnabled<Visible>(e, false);
        REQUIRE(storage.Has<Visible>(e));
        REQUIRE_FALSE(storage.IsEnabled<Visible>(e));
        REQUIRE(storage.View<Visible>().Empty());
        storage.SetEnabled<Visible>(e, true);
        REQUIRE(storage.View<Visible>().Size() == 1);
        REQUIRE(storage.Stats().StructuralMoves == before.StructuralMoves);
        REQUIRE(storage.Stats().Archetypes == before.Archetypes);

        // The flag moves with the row
        storage.SetEnabled<Visible>(e, false);
        storage.Add<Vel>(e);
        REQUIRE_FALSE(storage.IsEnabled<Visible>(e));
        REQUIRE(storage.Get<Visible>(e).Layer == 2);
    }
}

TEST_CASE("Hooks run on add, replace, remove, and destroy", "[ecs][storage]")
{
    struct Log
    {
        std::vector<std::string> Calls;
        void Added(EntityStorage&, Entity) { Calls.push_back("add"); }
        void Replaced(EntityStorage&, Entity) { Calls.push_back("replace"); }
        void Removed(EntityStorage& s, Entity e) { Calls.push_back(s.Has<Label>(e) ? "remove" : "remove-gone"); }
    } log;

    EntityStorage storage;
    storage.OnAdd<Label>().Connect<&Log::Added>(log);
    storage.OnReplace<Label>().Connect<&Log::Replaced>(log);
    storage.OnRemove<Label>().Connect<&Log::Removed>(log);

    const Entity e = storage.Create();
    storage.Add<Label>(e, Label{"x"});
    storage.AddOrReplace<Label>(e, Label{"y"});
    storage.Remove<Label>(e);
    storage.Add<Label>(e, Label{"z"});
    storage.Destroy(e);
    // Removed runs while the component is still there
    REQUIRE(log.Calls == std::vector<std::string>{"add", "replace", "remove", "add", "remove"});
}

TEST_CASE("Five systems each adding a component to one entity cause one archetype move", "[ecs][storage]")
{
    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<AddPartSystem<0>>();
    systems.Add<AddPartSystem<1>>();
    systems.Add<AddPartSystem<2>>();
    systems.Add<AddPartSystem<3>>();
    systems.Add<AddPartSystem<4>>();
    const Entity e = storage.Create();
    storage.Add<Pos>(e);
    std::unique_ptr<SystemRunner> runner = StartRunner(storage, systems);

    const uint64 movesBefore = storage.Stats().StructuralMoves;
    runner->RunTick(WorldRole::Play);
    REQUIRE(storage.Stats().StructuralMoves == movesBefore + 1);
    REQUIRE(storage.Get<Part<0>>(e).Value == 0);
    REQUIRE(storage.Get<Part<4>>(e).Value == 40);

    // The next tick replaces the values: no move
    runner->RunTick(WorldRole::Play);
    REQUIRE(storage.Stats().StructuralMoves == movesBefore + 1);
    runner->Destroy();
}

TEST_CASE("An entity spawned while a query runs is not seen by that loop", "[ecs][storage]")
{
    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<SpawnWhileIteratingSystem>();
    for (int i = 0; i < 3; ++i)
    {
        storage.Add<Pos>(storage.Create(), Pos{0.0f});
    }
    std::unique_ptr<SystemRunner> runner = StartRunner(storage, systems);

    runner->RunTick(WorldRole::Play);
    // Three were seen and moved, and three were spawned
    REQUIRE(storage.Count<Pos>() == 6);
    int moved = 0;
    storage.View<Pos>().Each([&](Pos& p) { moved += p.X == 1.0f ? 1 : 0; });
    REQUIRE(moved == 3);

    // The next tick sees all six
    runner->RunTick(WorldRole::Play);
    REQUIRE(storage.Count<Pos>() == 12);
    runner->Destroy();
}

TEST_CASE("A query keeps up with archetypes made after its first use", "[ecs][storage]")
{
    EntityStorage storage;
    const Entity a = storage.Create();
    storage.Add<Pos>(a);
    REQUIRE(storage.View<Pos>().Size() == 1);

    // A new archetype that also has Pos
    const Entity b = storage.Create();
    storage.Add<Pos>(b);
    storage.Add<Label>(b);
    REQUIRE(storage.View<Pos>().Size() == 2);
    REQUIRE(storage.View<Pos, Label>().Size() == 1);
}

TEST_CASE("Storage benchmarks", "[!benchmark][ecs][storage]")
{
    constexpr int EntityCount = 1000000;
    EntityStorage storage;
    for (int i = 0; i < EntityCount; ++i)
    {
        const Entity e = storage.Create();
        storage.Add<Pos>(e, Pos{static_cast<float>(i)});
        storage.Add<Vel>(e, Vel{1.0f});
        if (i % 2 == 0)
        {
            storage.Add<Label>(e);
        }
    }

    BENCHMARK("Iterate 1M entities, one component")
    {
        float sum = 0.0f;
        storage.View<Pos>().Each([&](const Pos& p) { sum += p.X; });
        return sum;
    };

    BENCHMARK("Iterate 1M entities, two components")
    {
        float sum = 0.0f;
        storage.View<Pos, Vel>().Each([&](const Pos& p, const Vel& v) { sum += p.X * v.X; });
        return sum;
    };

    BENCHMARK("Create and destroy 10k entities")
    {
        std::vector<Entity> made;
        made.reserve(10000);
        for (int i = 0; i < 10000; ++i)
        {
            const Entity e = storage.Create();
            storage.Add<Pos>(e);
            made.push_back(e);
        }
        for (const Entity e : made)
        {
            storage.Destroy(e);
        }
        return made.size();
    };
}

TEST_CASE("QueryRange Each visits the same entities as the range-for", "[ecs][storage]")
{
    EntityStorage storage;
    std::vector<Entity> made;
    for (int i = 0; i < 3000; ++i)
    {
        const Entity e = storage.Create();
        storage.Add<Pos>(e, Pos{static_cast<float>(i)});
        if (i % 2 == 0)
        {
            storage.Add<Vel>(e, Vel{1.0f});
        }
        if (i % 3 == 0)
        {
            storage.Add<Burning>(e, Burning{i});
        }
        if (i % 5 == 0)
        {
            storage.Add<Visible>(e, Visible{i});
            storage.SetEnabled<Visible>(e, i % 10 == 0);
        }
        made.push_back(e);
    }

    auto collect = [&](auto range, bool useEach)
    {
        std::vector<Entity> seen;
        if (useEach)
        {
            range.Each([&](const Entity e, auto&...) { seen.push_back(e); });
        }
        else
        {
            for (auto item : range)
            {
                seen.push_back(std::get<0>(item));
            }
        }
        std::sort(seen.begin(), seen.end());
        return seen;
    };

    using Plain = QueryRange<TypeList<const Pos, Vel>, TypeList<>, TypeList<>>;
    using WithSparse = QueryRange<TypeList<const Pos, Burning>, TypeList<>, TypeList<>>;
    using WithEnableable = QueryRange<TypeList<Pos>, TypeList<Visible>, TypeList<Burning>>;

    for (const bool useEach : {false, true})
    {
        const std::vector<Entity> plain = collect(Plain(storage.Native()), useEach);
        REQUIRE(plain.size() == 1500);
        const std::vector<Entity> sparse = collect(WithSparse(storage.Native()), useEach);
        REQUIRE(sparse.size() == 1000);
        // Visible on (i % 10 == 0) and no Burning (i % 3 != 0)
        size_t expected = 0;
        for (int i = 0; i < 3000; ++i)
        {
            expected += (i % 10 == 0 && i % 3 != 0) ? 1 : 0;
        }
        REQUIRE(collect(WithEnableable(storage.Native()), useEach).size() == expected);
    }

    // Each passes the right values, and writes go to the storage
    QueryRange<TypeList<Pos, const Burning>, TypeList<>, TypeList<>>(storage.Native())
        .Each([](Pos& p, const Burning& b) { p.X = static_cast<float>(b.Ticks) + 0.5f; });
    for (int i = 0; i < 3000; i += 3)
    {
        REQUIRE(storage.Get<Pos>(made[i]).X == static_cast<float>(i) + 0.5f);
    }
}
