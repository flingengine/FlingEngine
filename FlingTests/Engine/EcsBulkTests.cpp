#include "Ecs/EntityStorage.h"
#include "Ecs/SystemContext.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

namespace
{
    struct Pos
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Pos"};
        float X = 0.0f;
        float Y = 0.0f;
        float Z = 0.0f;
    };

    struct Vel
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Vel"};
        float X = 0.0f;
    };

    struct Health
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Health"};
        int Value = 100;
    };

    struct Frozen
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Frozen", .Kind = ComponentKind::Tag};
    };

    /** Not trivially copyable, and counts the live values, to find a leak or a double destroy. */
    struct Label
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Label"};
        static inline int Live = 0;
        String Text;

        Label() { ++Live; }
        explicit Label(String text) : Text(std::move(text)) { ++Live; }
        Label(const Label& other) : Text(other.Text) { ++Live; }
        Label(Label&& other) noexcept : Text(std::move(other.Text)) { ++Live; }
        Label& operator=(const Label&) = default;
        Label& operator=(Label&&) noexcept = default;
        ~Label() { --Live; }
    };

    struct Visible
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Visible", .Storage = ComponentStorage::Enableable};
        int Layer = 0;
    };

    struct Burning
    {
        static constexpr ComponentInfo Component{.Name = "Bulk.Burning", .Storage = ComponentStorage::Sparse};
        int Ticks = 0;
    };

    struct AddCounter
    {
        int Added = 0;
        int Removed = 0;

        void OnAdded(EntityStorage&, Entity) { ++Added; }

        void OnRemoved(EntityStorage&, Entity) { ++Removed; }
    };
} // namespace

TEST_CASE("AddToAll and RemoveFromAll move whole archetypes", "[ecs][storage][bulk]")
{
    EntityStorage storage;
    // More than one chunk in each archetype, so runs cross chunk boundaries on both sides
    constexpr int Count = 6000;
    std::vector<Entity> posOnly;
    std::vector<Entity> posVel;
    std::vector<Entity> posFrozen;
    std::vector<Entity> velOnly;
    for (int i = 0; i < Count; ++i)
    {
        const Entity a = storage.Create();
        storage.Add<Pos>(a, Pos{float(i), 1.0f, 2.0f});
        posOnly.push_back(a);

        const Entity b = storage.Create();
        storage.Add<Pos>(b, Pos{float(i), 0.0f, 0.0f});
        storage.Add<Vel>(b, Vel{float(-i)});
        posVel.push_back(b);

        const Entity c = storage.Create();
        storage.Add<Pos>(c);
        storage.Add<Frozen>(c);
        posFrozen.push_back(c);

        const Entity d = storage.Create();
        storage.Add<Vel>(d);
        velOnly.push_back(d);
    }
    // A few entities already in a target archetype, so the moved rows go after them
    const Entity early = storage.Create();
    storage.Add<Pos>(early, Pos{-1.0f, 0.0f, 0.0f});
    storage.Add<Health>(early, Health{5});

    const size_t added = storage.AddToAll(Health{7}, With<Pos>{}, Without<Frozen>{});
    CHECK(added == 2 * Count);

    for (int i = 0; i < Count; ++i)
    {
        REQUIRE(storage.Has<Health>(posOnly[i]));
        CHECK(storage.Get<Health>(posOnly[i]).Value == 7);
        CHECK(storage.Get<Pos>(posOnly[i]).X == float(i));
        CHECK(storage.Get<Pos>(posOnly[i]).Z == 2.0f);

        REQUIRE(storage.Has<Health>(posVel[i]));
        CHECK(storage.Get<Vel>(posVel[i]).X == float(-i));
        CHECK(storage.Get<Pos>(posVel[i]).X == float(i));

        CHECK_FALSE(storage.Has<Health>(posFrozen[i]));
        CHECK_FALSE(storage.Has<Health>(velOnly[i]));
    }
    CHECK(storage.Get<Health>(early).Value == 5);
    CHECK(storage.Count<Health>() == 2 * Count + 1);

    // A query sees every moved entity once
    size_t seen = 0;
    float sum = 0.0f;
    QueryRange<TypeList<Pos, Health>, TypeList<>, TypeList<>>(storage.Native())
        .Each(
            [&](Pos& pos, Health& health)
            {
                ++seen;
                sum += float(health.Value);
                (void)pos;
            });
    CHECK(seen == 2 * Count + 1);
    CHECK(sum == float(7 * 2 * Count + 5));

    // Nothing left to add to: the second call does nothing
    CHECK(storage.AddToAll(Health{9}, With<Pos>{}, Without<Frozen>{}) == 0);

    const size_t removed = storage.RemoveFromAll<Health>(With<Vel>{});
    CHECK(removed == Count);
    for (int i = 0; i < Count; ++i)
    {
        CHECK_FALSE(storage.Has<Health>(posVel[i]));
        CHECK(storage.Get<Vel>(posVel[i]).X == float(-i));
        CHECK(storage.Has<Health>(posOnly[i]));
    }

    // No filters: every entity with Health
    CHECK(storage.RemoveFromAll<Health>() == Count + 1);
    CHECK(storage.Count<Health>() == 0);

    // A tag, added to the entities that have Vel
    CHECK(storage.AddToAll(Frozen{}, With<Vel>{}) == 2 * Count);
    CHECK(storage.Has<Frozen>(velOnly[17]));
    CHECK(storage.Get<Vel>(posVel[17]).X == -17.0f);

    // Single adds and removes still work on the moved entities
    storage.Remove<Frozen>(posVel[3]);
    storage.Add<Health>(posVel[3], Health{3});
    CHECK(storage.Get<Health>(posVel[3]).Value == 3);
    storage.Destroy(posVel[4]);
    CHECK(storage.Get<Vel>(posVel[5]).X == -5.0f);
}

TEST_CASE("AddToAll moves values that are not trivial, and keeps enabled flags", "[ecs][storage][bulk]")
{
    Label::Live = 0;
    {
        EntityStorage storage;
        std::vector<Entity> entities;
        for (int i = 0; i < 3000; ++i)
        {
            const Entity e = storage.Create();
            storage.Add<Label>(e, Label{"entity " + std::to_string(i)});
            storage.Add<Visible>(e, Visible{i});
            storage.SetEnabled<Visible>(e, i % 3 != 0);
            entities.push_back(e);
        }
        CHECK(Label::Live == 3000);

        CHECK(storage.AddToAll(Pos{}, With<Label>{}) == 3000);
        CHECK(Label::Live == 3000);
        for (int i = 0; i < 3000; ++i)
        {
            CHECK(storage.Get<Label>(entities[i]).Text == "entity " + std::to_string(i));
            CHECK(storage.Get<Visible>(entities[i]).Layer == i);
            CHECK(storage.IsEnabled<Visible>(entities[i]) == (i % 3 != 0));
        }

        // Removing the non-trivial type ends each value once
        CHECK(storage.RemoveFromAll<Label>() == 3000);
        CHECK(Label::Live == 0);
        CHECK(storage.IsEnabled<Visible>(entities[3]) == false);
        CHECK(storage.IsEnabled<Visible>(entities[4]) == true);

        // A non-trivial value copied to every entity
        CHECK(storage.AddToAll(Label{"same"}, With<Visible>{}) == 3000);
        CHECK(Label::Live == 3000);
        CHECK(storage.Get<Label>(entities[2999]).Text == "same");

        // An added Enableable starts enabled
        storage.RemoveFromAll<Visible>();
        CHECK(storage.AddToAll(Visible{4}, With<Pos>{}) == 3000);
        CHECK(storage.IsEnabled<Visible>(entities[0]));
        CHECK(storage.Get<Visible>(entities[0]).Layer == 4);
    }
    CHECK(Label::Live == 0);
}

TEST_CASE("AddToAll does a Sparse type, or a type with hooks, one entity at a time", "[ecs][storage][bulk]")
{
    EntityStorage storage;
    std::vector<Entity> entities;
    for (int i = 0; i < 100; ++i)
    {
        const Entity e = storage.Create();
        storage.Add<Pos>(e);
        if (i % 2 == 0)
        {
            storage.Add<Vel>(e);
        }
        entities.push_back(e);
    }
    storage.Add<Burning>(entities[0], Burning{1});

    CHECK(storage.AddToAll(Burning{3}, With<Vel>{}) == 49); // entities[0] has one already
    CHECK(storage.Get<Burning>(entities[0]).Ticks == 1);
    CHECK(storage.Get<Burning>(entities[2]).Ticks == 3);
    CHECK_FALSE(storage.Has<Burning>(entities[1]));
    CHECK(storage.RemoveFromAll<Burning>(With<Pos>{}) == 50);
    CHECK(storage.Count<Burning>() == 0);

    AddCounter counter;
    storage.OnAdd<Health>().Connect<&AddCounter::OnAdded>(counter);
    storage.OnRemove<Health>().Connect<&AddCounter::OnRemoved>(counter);
    CHECK(storage.AddToAll(Health{2}, With<Pos>{}, Without<Vel>{}) == 50);
    CHECK(counter.Added == 50);
    CHECK(storage.Get<Health>(entities[1]).Value == 2);
    CHECK(storage.RemoveFromAll<Health>() == 50);
    CHECK(counter.Removed == 50);
}

namespace
{
    /** Checks, from an Added hook, that every component of a one-step Create is in place. */
    struct CreateCheck
    {
        int Added = 0;
        int SawAll = 0;

        void OnAdded(EntityStorage& storage, const Entity e)
        {
            ++Added;
            if (storage.Has<Pos, Health, Frozen, Burning>(e) && storage.Get<Health>(e).Value == 11)
            {
                ++SawAll;
            }
        }
    };
} // namespace

TEST_CASE("Create with components puts the entity in its archetype in one step", "[ecs][storage]")
{
    EntityStorage storage;
    CreateCheck check;
    storage.OnAdd<Pos>().Connect<&CreateCheck::OnAdded>(check);
    storage.OnAdd<Burning>().Connect<&CreateCheck::OnAdded>(check);

    const uint64 movesBefore = storage.Stats().StructuralMoves;
    std::vector<Entity> entities;
    for (int i = 0; i < 2000; ++i)
    {
        entities.push_back(storage.Create(Pos{float(i), 0.0f, 0.0f}, Health{11}, Frozen{}, Burning{i}));
    }
    // No entity moved between archetypes on the way
    CHECK(storage.Stats().StructuralMoves == movesBefore);
    CHECK(check.Added == 4000);
    CHECK(check.SawAll == 4000);

    CHECK(storage.Get<Pos>(entities[1234]).X == 1234.0f);
    CHECK(storage.Get<Burning>(entities[1234]).Ticks == 1234);
    CHECK(storage.Has<Frozen>(entities[5]));

    // The same archetype as Create and then Add for each
    const Entity stepwise = storage.Create();
    storage.Add<Health>(stepwise, Health{11});
    storage.Add<Frozen>(stepwise);
    storage.Add<Pos>(stepwise);
    size_t inArchetype = 0;
    QueryRange<TypeList<Pos, Health>, TypeList<Frozen>, TypeList<>>(storage.Native())
        .Each([&](Pos&, Health&) { ++inArchetype; });
    CHECK(inArchetype == 2001);

    // A non-trivial value is moved in, and ends when the entity does
    Label::Live = 0;
    const Entity labeled = storage.Create(Label{"made"}, Vel{2.0f});
    CHECK(storage.Get<Label>(labeled).Text == "made");
    CHECK(Label::Live == 1);
    storage.Destroy(labeled);
    CHECK(Label::Live == 0);
}
