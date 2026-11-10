#include "Components/Transform.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/TypeRegistry.h"
#include "Game.h"
#include "Json.h"
#include "Logger.h"
#include "World.h"

#include <algorithm>
#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>

// Components that require other components (Fling::RequiredOf): the compile-time
// rules, what the type registry knows, and what the editor and level load do with it.
// The rules that must not compile are in FlingTests/EcsCompileFail.

using namespace Fling;

namespace
{
    struct Place
    {
        static constexpr ComponentInfo Component{.Name = "Req.Place"};
        float X = 0.0f;
    };

    /** Requires Place. */
    struct Lamp
    {
        static constexpr ComponentInfo Component{.Name = "Req.Lamp"};
        using Requires = TypeList<Place>;
        float Power = 1.0f;
    };

    /** Requires Lamp, and so Place too. */
    struct Flicker
    {
        static constexpr ComponentInfo Component{.Name = "Req.Flicker"};
        using Requires = TypeList<Lamp>;
        float Rate = 2.0f;
    };

    // A loop of three. The compiler finds a loop of two. The type registry finds a longer one.
    struct LoopB;
    struct LoopC;
    struct LoopA
    {
        static constexpr ComponentInfo Component{.Name = "Req.LoopA"};
        using Requires = TypeList<LoopB>;
        int Value = 0;
    };
    struct LoopB
    {
        static constexpr ComponentInfo Component{.Name = "Req.LoopB"};
        using Requires = TypeList<LoopC>;
        int Value = 0;
    };
    struct LoopC
    {
        static constexpr ComponentInfo Component{.Name = "Req.LoopC"};
        using Requires = TypeList<LoopA>;
        int Value = 0;
    };

    void RegisterLamps(ModuleRegistration& modules)
    {
        modules.Component<Place>([](TypeBuilder<Place>& type) { type.Field("x", &Place::X); });
        modules.Component<Lamp>([](TypeBuilder<Lamp>& type) { type.Field("power", &Lamp::Power); });
        modules.Component<Flicker>([](TypeBuilder<Flicker>& type) { type.Field("rate", &Flicker::Rate); });
    }

    void QuietLogger()
    {
        Logger::Get().Init();
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    class LampGame : public Game
    {
    public:
        void Init(EntityStorage&) override {}

        void Register(ModuleRegistration& t_Modules) override
        {
            RegisterEngine(t_Modules);
            RegisterLamps(t_Modules);
        }

        void OnStartGame(EntityStorage&) override {}

        void Update(EntityStorage&, float) override {}

        void OnStopGame(EntityStorage&) override {}

        void Shutdown(EntityStorage&) override {}
    };
} // namespace

TEST_CASE("A component declares what it requires", "[ecs][requires]")
{
    STATIC_REQUIRE(std::is_same_v<RequiredOf<Place>, TypeList<>>);
    STATIC_REQUIRE(std::is_same_v<RequiredOf<Lamp>, TypeList<Place>>);

    // What Create and ctx.Spawn check: each component has what it requires in the same call
    STATIC_REQUIRE(HasRequired<Place, Place>);
    STATIC_REQUIRE(HasRequired<Lamp, Place, Lamp>);
    STATIC_REQUIRE_FALSE(HasRequired<Lamp, Lamp>);
    STATIC_REQUIRE_FALSE(HasRequired<Flicker, Place, Flicker>);

    // In any order in one Create, and one at a time when the required one is first
    EntityStorage storage;
    const Entity whole = storage.Create(Flicker{}, Lamp{}, Place{});
    REQUIRE(storage.Has<Place, Lamp, Flicker>(whole));

    const Entity built = storage.Create();
    storage.Add<Place>(built);
    storage.Add<Lamp>(built);
    // Removed in the other order: the one that requires first
    storage.Remove<Lamp>(built);
    storage.Remove<Place>(built);
    REQUIRE_FALSE(storage.Has<Place>(built));

    // To destroy an entity needs no order
    storage.Destroy(whole);
    REQUIRE_FALSE(storage.IsValid(whole));
}

TEST_CASE("The type registry knows what each type requires", "[ecs][requires]")
{
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterLamps(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);
    REQUIRE(types.Ok());

    const TypeDesc& place = *types.Find<Place>();
    const TypeDesc& lamp = *types.Find<Lamp>();
    const TypeDesc& flicker = *types.Find<Flicker>();
    REQUIRE(place.Requires.empty());
    REQUIRE(lamp.Requires.size() == 1);
    REQUIRE(lamp.Requires[0].Hash == TypeHashOf<Place>);

    // The order to add them in: a type after the types that it requires
    REQUIRE(types.WithRequired(place) == std::vector<const TypeDesc*>{&place});
    REQUIRE(types.WithRequired(flicker) == std::vector<const TypeDesc*>{&place, &lamp, &flicker});

    SECTION("The editor adds a component with what it requires")
    {
        EntityStorage storage;
        const Entity e = storage.Create();
        types.AddWithRequired(storage, e, flicker);
        REQUIRE(storage.Has<Place, Lamp, Flicker>(e));

        // What the entity has already keeps its values
        const Entity placed = storage.Create(Place{7.0f});
        types.AddWithRequired(storage, placed, lamp);
        REQUIRE(storage.Get<Place>(placed).X == 7.0f);
        REQUIRE(storage.Has<Lamp>(placed));
    }

    SECTION("The editor does not remove a component that another one requires")
    {
        EntityStorage storage;
        const Entity e = storage.Create(Place{}, Lamp{}, Flicker{});
        REQUIRE(types.RequiredBy(storage, e, place) == &lamp);
        REQUIRE(types.RequiredBy(storage, e, lamp) == &flicker);
        REQUIRE(types.RequiredBy(storage, e, flicker) == nullptr);

        // Top down, each one becomes free to remove
        flicker.Remove(storage, e);
        REQUIRE(types.RequiredBy(storage, e, lamp) == nullptr);
        lamp.Remove(storage, e);
        REQUIRE(types.RequiredBy(storage, e, place) == nullptr);

        // Another entity's components do not count
        const Entity other = storage.Create(Place{}, Lamp{});
        (void)other;
        REQUIRE(types.RequiredBy(storage, e, place) == nullptr);
    }
}

TEST_CASE("A required type that is not registered, and a loop of requirements, are errors", "[ecs][requires]")
{
    QuietLogger();
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Lamp>();
        const TypeRegistry types = TypeRegistry::Build(modules);
        REQUIRE_FALSE(types.Ok());
        REQUIRE(types.Errors().front().find("'Req.Lamp' requires 'Req.Place', which is not registered") !=
                String::npos);
    }
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<LoopA>();
        modules.Component<LoopB>();
        modules.Component<LoopC>();
        const TypeRegistry types = TypeRegistry::Build(modules);
        REQUIRE(types.Errors().size() == 1);
        REQUIRE(types.Errors().front().find("Req.LoopA -> Req.LoopB -> Req.LoopC -> Req.LoopA") != String::npos);

        // A loop does not hang the code that walks the requirements
        REQUIRE(types.WithRequired(*types.Find<LoopA>()).size() == 3);
    }
}

TEST_CASE("A level loads a component after what it requires, and adds what is missing", "[ecs][requires][world]")
{
    QuietLogger();
    const auto gameOwner = std::make_shared<LampGame>();
    LampGame& game = *gameOwner;
    World world(gameOwner);
    REQUIRE(world.Init());
    EntityStorage& storage = world.GetStorage();

    // What the Added hook of Lamp sees: the Place of the same entity must be there, with its saved value
    struct Seen
    {
        std::vector<float> PlaceX;
        void OnLamp(EntityStorage& s, const Entity e)
        {
            const Place* place = s.TryGet<Place>(e);
            PlaceX.push_back(place != nullptr ? place->X : -1.0f);
        }
    } seen;
    storage.OnAdd<Lamp>().Connect<&Seen::OnLamp>(seen);

    // "Req.Lamp" sorts before "Req.Place" in the file. The second entity has no Place at all.
    const Json level = Json::Parse(R"({
        "title": "Requires",
        "entities": [
            { "name": "Whole", "Req.Flicker": { "rate": 9.0 }, "Req.Lamp": { "power": 3.0 }, "Req.Place": { "x": 5.0 } },
            { "name": "Lacking", "Req.Lamp": { "power": 4.0 } }
        ]
    })");
    REQUIRE(world.LoadFromJson(level, "RequiresTest"));

    REQUIRE(seen.PlaceX == std::vector<float>{5.0f, 0.0f});
    REQUIRE(storage.Count<Place>() == 2);
    REQUIRE(storage.Count<Lamp>() == 2);
    REQUIRE(storage.Count<Flicker>() == 1);
    storage.View<Flicker>().Each(
        [&](const Entity e, Flicker& flicker)
        {
            REQUIRE(flicker.Rate == 9.0f);
            REQUIRE(storage.Get<Lamp>(e).Power == 3.0f);
            REQUIRE(storage.Get<Place>(e).X == 5.0f);
        });

    storage.OnAdd<Lamp>().Disconnect<&Seen::OnLamp>(seen);
    world.Shutdown();
}
