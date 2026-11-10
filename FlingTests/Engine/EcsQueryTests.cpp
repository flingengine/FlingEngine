#include "Ecs/EntityStorage.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <catch2/catch_all.hpp>
#include <thread>
#include <vector>

// Queries: how they find their entities (through the archetypes, or through a sparse
// set), what they cost as the world grows, and that systems on different threads can
// make them at the same time.

using namespace Fling;

namespace
{
    struct Pos
    {
        static constexpr ComponentInfo Component{.Name = "Query.Pos"};
        float X = 0.0f;
    };

    struct Vel
    {
        static constexpr ComponentInfo Component{.Name = "Query.Vel"};
        float X = 0.0f;
    };

    struct Frozen
    {
        static constexpr ComponentInfo Component{.Name = "Query.Frozen", .Kind = ComponentKind::Tag};
    };

    struct Burning
    {
        static constexpr ComponentInfo Component{.Name = "Query.Burning", .Storage = ComponentStorage::Sparse};
        int Ticks = 0;
    };

    struct Wet
    {
        static constexpr ComponentInfo Component{.Name = "Query.Wet", .Storage = ComponentStorage::Sparse};
        int Amount = 0;
    };

    struct Visible
    {
        static constexpr ComponentInfo Component{.Name = "Query.Visible", .Storage = ComponentStorage::Enableable};
        int Layer = 0;
    };

    /** Many types, to make many archetypes and many cached queries. */
    template <int N>
    struct Kind
    {
        static constexpr ComponentInfo Component{.Name = N == 0   ? "Query.Kind0"
                                                         : N == 1 ? "Query.Kind1"
                                                         : N == 2 ? "Query.Kind2"
                                                         : N == 3 ? "Query.Kind3"
                                                         : N == 4 ? "Query.Kind4"
                                                         : N == 5 ? "Query.Kind5"
                                                         : N == 6 ? "Query.Kind6"
                                                                  : "Query.Kind7"};
        int Value = N;
    };

    template <typename... Ts, typename... Ws, typename... Xs>
    std::vector<Entity> Collect(EntityStorage& storage, TypeList<Ts...>, TypeList<Ws...>, TypeList<Xs...>, bool useEach)
    {
        std::vector<Entity> seen;
        QueryRange<TypeList<Ts...>, TypeList<Ws...>, TypeList<Xs...>> range(storage.Native());
        if (useEach)
        {
            range.Each([&](const Entity entity, Ts&...) { seen.push_back(entity); });
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
    }

    /** Spawns from inside a loop over a sparse component. */
    class SpreadFireSystem final : public System<SpreadFireSystem, Access<Write<Pos>, Write<Burning>, Spawns>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Query.SpreadFire", .Phase = Phases::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            for (auto [entity, pos, burning] : ctx.Query<Pos, Burning>())
            {
                ++burning.Ticks;
                ctx.Spawn(Pos{pos.X + 100.0f}, Burning{0});
            }
        }
    };
} // namespace

TEST_CASE("A query that needs a Sparse component walks the sparse set", "[ecs][query]")
{
    EntityStorage storage;
    std::vector<Entity> burning;
    std::vector<Entity> burningAndMoving;
    std::vector<Entity> burningNotFrozen;
    std::vector<Entity> burningAndWet;
    for (int i = 0; i < 5000; ++i)
    {
        const Entity e = storage.Create(Pos{float(i)});
        if (i % 2 == 0)
        {
            storage.Add<Vel>(e, Vel{1.0f});
        }
        if (i % 7 == 0)
        {
            storage.Add<Frozen>(e);
        }
        if (i % 50 == 0)
        {
            storage.Add<Burning>(e, Burning{i});
            burning.push_back(e);
            if (i % 2 == 0)
            {
                burningAndMoving.push_back(e);
            }
            if (i % 7 != 0)
            {
                burningNotFrozen.push_back(e);
            }
        }
        if (i % 150 == 0)
        {
            storage.Add<Wet>(e, Wet{i});
            burningAndWet.push_back(e);
        }
    }
    // An entity with the sparse component and no Pos is not in a query for Pos
    storage.Add<Burning>(storage.Create(), Burning{-1});
    std::sort(burning.begin(), burning.end());
    std::sort(burningAndMoving.begin(), burningAndMoving.end());
    std::sort(burningNotFrozen.begin(), burningNotFrozen.end());
    std::sort(burningAndWet.begin(), burningAndWet.end());

    for (const bool useEach : {false, true})
    {
        REQUIRE(Collect(storage, TypeList<Pos, const Burning>{}, TypeList<>{}, TypeList<>{}, useEach) == burning);
        // The sparse type only as a filter
        REQUIRE(Collect(storage, TypeList<const Pos>{}, TypeList<Burning>{}, TypeList<>{}, useEach) == burning);
        REQUIRE(Collect(storage, TypeList<Pos, Vel>{}, TypeList<Burning>{}, TypeList<>{}, useEach) == burningAndMoving);
        REQUIRE(Collect(storage, TypeList<Burning>{}, TypeList<Pos>{}, TypeList<Frozen>{}, useEach) ==
                burningNotFrozen);
        // Two sparse types: the smaller set is walked, and the result is the same
        REQUIRE(Collect(storage, TypeList<Burning, Wet>{}, TypeList<Pos>{}, TypeList<>{}, useEach) == burningAndWet);
        REQUIRE(Collect(storage, TypeList<Wet, Burning>{}, TypeList<Pos>{}, TypeList<>{}, useEach) == burningAndWet);
        // A sparse type that only excludes still walks the archetypes
        REQUIRE(Collect(storage, TypeList<Pos>{}, TypeList<>{}, TypeList<Burning>{}, useEach).size() ==
                5000 - burning.size());
    }

    // The values are the entity's own, and writes go to the storage
    QueryRange<TypeList<Pos, const Burning>, TypeList<>, TypeList<>>(storage.Native())
        .Each([](Pos& pos, const Burning& fire) { pos.X = float(fire.Ticks) + 0.25f; });
    for (size_t i = 0; i < burning.size(); ++i)
    {
        REQUIRE(storage.Get<Pos>(burning[i]).X == float(storage.Get<Burning>(burning[i]).Ticks) + 0.25f);
    }
}

TEST_CASE("A query for a Sparse type that no entity ever had is empty", "[ecs][query]")
{
    EntityStorage storage;
    storage.Create(Pos{1.0f});
    for (const bool useEach : {false, true})
    {
        REQUIRE(Collect(storage, TypeList<Pos, Wet>{}, TypeList<>{}, TypeList<>{}, useEach).empty());
    }
}

TEST_CASE("A loop over a sparse component does not see the entities it spawns", "[ecs][query]")
{
    EntityStorage storage;
    SystemRegistry systems;
    systems.Add<SpreadFireSystem>();
    ScheduleResult schedule = systems.BuildSchedule();
    REQUIRE(schedule.ErrorReport() == "");
    SystemRunner runner(systems, std::move(schedule.Value), storage);
    runner.Create();

    for (int i = 0; i < 10; ++i)
    {
        storage.Create(Pos{float(i)}, Burning{0});
    }
    storage.Create(Pos{-1.0f});

    runner.RunTick(WorldRole::Play);
    // Each of the 10 spawned one, and the loop did not run for the new ones
    REQUIRE(storage.Count<Burning>() == 20);
    size_t ticked = 0;
    storage.View<Burning>().Each([&](Burning& fire) { ticked += fire.Ticks == 1 ? 1 : 0; });
    REQUIRE(ticked == 10);

    runner.RunTick(WorldRole::Play);
    REQUIRE(storage.Count<Burning>() == 40);
    runner.Destroy();
}

TEST_CASE("A cached query gets the archetypes that are made after it", "[ecs][query]")
{
    EntityStorage storage;
    const auto count = [&]
    {
        size_t rows = 0;
        QueryRange<TypeList<const Pos>, TypeList<>, TypeList<Frozen>>(storage.Native())
            .Each([&](const Pos&) { ++rows; });
        return rows;
    };
    REQUIRE(count() == 0);

    storage.Create(Pos{1.0f});
    REQUIRE(count() == 1);
    // New archetypes: one matches, and one is excluded
    storage.Create(Pos{2.0f}, Vel{1.0f});
    storage.Create(Pos{3.0f}, Frozen{});
    REQUIRE(count() == 2);
    storage.Create(Pos{4.0f}, Vel{1.0f}, Visible{1});
    REQUIRE(count() == 3);

    // A query that is new sees the archetypes that exist already
    size_t moving = 0;
    QueryRange<TypeList<const Pos, const Vel>, TypeList<>, TypeList<>>(storage.Native())
        .Each([&](const Pos&, const Vel&) { ++moving; });
    REQUIRE(moving == 2);
}

TEST_CASE("Rows fill the chunks of an archetype in order when rows come and go", "[ecs][query][storage]")
{
    // How many rows one archetype holds when it first needs a fourth chunk. With churn, rows are
    // destroyed and made again at the point where the third chunk has one row.
    const auto rowsAtFourthChunk = [](const bool churn)
    {
        EntityStorage storage;
        std::vector<Entity> alive;
        while (storage.Stats().Chunks < 3)
        {
            alive.push_back(storage.Create(Pos{0.0f}));
        }
        if (churn)
        {
            // Empty the third chunk (it stays as a spare), take 10 rows from the second, then make
            // 11 rows. They must fill the second chunk first, not go in the spare after it.
            for (int i = 0; i < 11; ++i)
            {
                storage.Destroy(alive.back());
                alive.pop_back();
            }
            REQUIRE(storage.Stats().Chunks == 3);
            for (int i = 0; i < 11; ++i)
            {
                alive.push_back(storage.Create(Pos{0.0f}));
            }
        }
        while (storage.Stats().Chunks < 4)
        {
            alive.push_back(storage.Create(Pos{0.0f}));
        }

        // A query sees every row one time
        size_t rows = 0;
        QueryRange<TypeList<const Pos>, TypeList<>, TypeList<>>(storage.Native()).Each([&](const Pos&) { ++rows; });
        REQUIRE(rows == alive.size());
        return alive.size();
    };

    // No chunk is left part empty: the archetype holds as many rows as one that never lost a row
    REQUIRE(rowsAtFourthChunk(true) == rowsAtFourthChunk(false));
}

TEST_CASE("Destroy does not get slower as the number of Sparse types grows", "[ecs][query][storage]")
{
    // Destroy walks only the sparse sets that exist. An entity with none of them is destroyed, and one
    // with two of them loses both values.
    EntityStorage storage;
    const Entity plain = storage.Create(Pos{1.0f});
    const Entity both = storage.Create(Pos{2.0f}, Burning{3}, Wet{4});
    REQUIRE(storage.Count<Burning>() == 1);
    storage.Destroy(plain);
    REQUIRE(storage.Count<Burning>() == 1);
    storage.Destroy(both);
    REQUIRE(storage.Count<Burning>() == 0);
    REQUIRE(storage.Count<Wet>() == 0);

    // The slot is used again, and the old sparse values do not come back with it
    const Entity again = storage.Create(Pos{5.0f});
    REQUIRE(again.Index == both.Index);
    REQUIRE_FALSE(storage.Has<Burning>(again));
    REQUIRE_FALSE(storage.Has<Wet>(again));
}

TEST_CASE("A Sparse component on few entities of a big world takes little memory", "[ecs][query][storage][memory]")
{
    EntityStorage storage;
    std::vector<Entity> all;
    for (int i = 0; i < 300000; ++i)
    {
        all.push_back(storage.Create(Pos{float(i)}));
    }
    const size_t before = storage.Stats().UsedBytes;

    // The first, one in the middle, and the last: far apart in entity index
    const std::vector<Entity> burning{all.front(), all[150000], all.back()};
    for (const Entity e : burning)
    {
        storage.Add<Burning>(e, Burning{int(e.Index)});
    }
    // Three lookup pages of 16 KiB and one page of values, not 4 bytes for each of 300000 entities
    REQUIRE(storage.Stats().UsedBytes - before <= 4 * 16 * 1024);

    for (const Entity e : burning)
    {
        REQUIRE(storage.Get<Burning>(e).Ticks == int(e.Index));
    }
    REQUIRE_FALSE(storage.Has<Burning>(all[1]));
    REQUIRE_FALSE(storage.Has<Burning>(all[149999]));
    REQUIRE_FALSE(storage.Has<Burning>(all[299998]));

    // Remove the middle one: the last one takes its place in the dense list, and both lookups stay right
    storage.Remove<Burning>(all[150000]);
    REQUIRE_FALSE(storage.Has<Burning>(all[150000]));
    REQUIRE(storage.Get<Burning>(all.back()).Ticks == int(all.back().Index));
    REQUIRE(storage.Count<Burning>() == 2);

    // A destroyed entity leaves nothing for the entity that takes its index
    storage.Destroy(all.back());
    const Entity again = storage.Create(Pos{1.0f});
    REQUIRE(again.Index == all.back().Index);
    REQUIRE_FALSE(storage.Has<Burning>(again));
}

TEST_CASE("Threads can make and run queries on one storage at the same time", "[ecs][query][threads]")
{
    EntityStorage storage;
    constexpr int EntityCount = 4000;
    for (int i = 0; i < EntityCount; ++i)
    {
        const Entity e = storage.Create(Pos{1.0f});
        // Many archetypes
        if (i & 1)
            storage.Add<Kind<0>>(e);
        if (i & 2)
            storage.Add<Kind<1>>(e);
        if (i & 4)
            storage.Add<Kind<2>>(e);
        if (i & 8)
            storage.Add<Kind<3>>(e);
        if (i % 100 == 0)
            storage.Add<Burning>(e, Burning{1});
    }

    // Each thread reads only. Some queries are new to the storage's cache when two threads ask for them.
    constexpr int ThreadCount = 8;
    std::atomic<int> wrong{0};
    std::atomic<bool> go{false};
    std::vector<std::thread> threads;
    for (int t = 0; t < ThreadCount; ++t)
    {
        threads.emplace_back(
            [&]
            {
                while (!go.load())
                {
                    std::this_thread::yield();
                }
                for (int round = 0; round < 200; ++round)
                {
                    size_t all = 0;
                    size_t odd = 0;
                    size_t notOdd = 0;
                    size_t two = 0;
                    size_t burning = 0;
                    Detail::NativeStorage& native = storage.Native();
                    QueryRange<TypeList<const Pos>, TypeList<>, TypeList<>>(native).Each([&](const Pos&) { ++all; });
                    QueryRange<TypeList<const Pos, const Kind<0>>, TypeList<>, TypeList<>>(native).Each(
                        [&](const Pos&, const Kind<0>&) { ++odd; });
                    for (auto item : QueryRange<TypeList<const Pos>, TypeList<>, TypeList<Kind<0>>>(native))
                    {
                        (void)item;
                        ++notOdd;
                    }
                    {
                        // Two queries alive at once on this thread
                        QueryRange<TypeList<const Kind<1>>, TypeList<Kind<2>>, TypeList<>> outer(native);
                        for (auto item : outer)
                        {
                            (void)item;
                            ++two;
                        }
                        QueryRange<TypeList<const Pos, const Burning>, TypeList<>, TypeList<>>(native).Each(
                            [&](const Pos&, const Burning&) { ++burning; });
                    }
                    if (all != EntityCount || odd != EntityCount / 2 || notOdd != EntityCount / 2 ||
                        two != EntityCount / 4 || burning != EntityCount / 100)
                    {
                        ++wrong;
                    }
                }
            });
    }
    go = true;
    for (std::thread& thread : threads)
    {
        thread.join();
    }
    REQUIRE(wrong.load() == 0);
}

TEST_CASE("Query benchmarks", "[!benchmark][ecs][query]")
{
    constexpr int EntityCount = 200000;
    EntityStorage storage;
    for (int i = 0; i < EntityCount; ++i)
    {
        const Entity e = storage.Create(Pos{float(i)}, Vel{1.0f});
        if (i % 1000 == 0)
        {
            storage.Add<Burning>(e, Burning{1});
        }
    }

    // A big game: 2000 kinds of query in the storage's cache
    Detail::NativeStorage& native = storage.Native();
    for (Detail::ComponentId a = 0; a < 50; ++a)
    {
        for (Detail::ComponentId b = 0; b < 40; ++b)
        {
            const std::array<Detail::ComponentId, 2> required{a, a + b + 1};
            (void)native.Match(required, {});
        }
    }

    BENCHMARK("Make a query, with 2000 kinds of query cached")
    {
        QueryRange<TypeList<Pos, const Vel>, TypeList<>, TypeList<>> range(native);
        return range.begin() != range.end();
    };

    BENCHMARK("Iterate 200k entities, two components")
    {
        float sum = 0.0f;
        QueryRange<TypeList<Pos, const Vel>, TypeList<>, TypeList<>>(native).Each(
            [&](Pos& pos, const Vel& vel)
            {
                pos.X += vel.X;
                sum += pos.X;
            });
        return sum;
    };

    BENCHMARK("Iterate the 200 entities of 200k that have a Sparse component")
    {
        int sum = 0;
        QueryRange<TypeList<const Pos, const Burning>, TypeList<>, TypeList<>>(native).Each(
            [&](const Pos&, const Burning& fire) { sum += fire.Ticks; });
        return sum;
    };
}
