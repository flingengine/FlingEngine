#include "Ecs/EntityStorage.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemBase.h"
#include "Ecs/SystemContext.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Ecs/TypeRegistry.h"
#include "FlingTypes.h"
#include "Json.h"

#include <catch2/catch_all.hpp>
#include <memory>
#include <vector>

// The worked example in Docs/ecs-tutorial.md. The tutorial copies its code from this
// file, so keep the two the same: a change here needs the same change there.
//
// Poisoned entities lose health each tick. PoisonSystem asks for the damage, and
// DamageSystem, the one place where health goes down, applies it and destroys the dead.

using namespace Fling;

namespace
{
    // --- Step 1: the components ---------------------------------------------------

    /** Hit points. Saved in level files. */
    struct Health
    {
        static constexpr ComponentInfo Component{.Name = "Tutorial.Health"};

        float Current = 100.0f;
        float Max = 100.0f;
    };

    /** Damage over time. Saved in level files. */
    struct Poison
    {
        static constexpr ComponentInfo Component{.Name = "Tutorial.Poison"};

        float DamagePerSecond = 10.0f;
    };

    /** Tag: this entity takes no damage. A tag has no data. */
    struct Invulnerable
    {
        static constexpr ComponentInfo Component{.Name = "Tutorial.Invulnerable", .Kind = ComponentKind::Tag};
    };

    /** One request for damage. Not a component: it goes in the DamageRequests queue. */
    struct DamageRequest
    {
        Entity Target;
        float Amount = 0.0f;
    };

    /** Queue component: damage that DamageSystem applies this tick. */
    struct DamageRequests
    {
        static constexpr ComponentInfo Component{
            .Name = "Tutorial.DamageRequests", .Kind = ComponentKind::Queue, .Transient = true};

        std::vector<DamageRequest> Requests;
    };

    /** Singleton component: the state that DamageSystem keeps between ticks. */
    struct DamageStats
    {
        static constexpr ComponentInfo Component{
            .Name = "Tutorial.DamageStats", .Kind = ComponentKind::Singleton, .Transient = true};

        uint32 Kills = 0;
    };

    // --- Step 2: the systems ------------------------------------------------------
    // A real system is a header in inc/Systems/ and a body in src/Systems/. These
    // are in one test file so that the whole example is in one place.

    /** Asks for damage on each poisoned entity, every tick. */
    class PoisonSystem final : public System<PoisonSystem, Access<Read<SimTime>, Read<Poison>, Append<DamageRequests>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Tutorial.Poison", .Phase = Phases::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const float dt = ctx.Singleton<const SimTime>().FixedDelta;
            for (auto [entity, poison] : ctx.Query<const Poison>())
            {
                ctx.Queue<DamageRequests>().Push(DamageRequest{entity, poison.DamagePerSecond * dt});
            }
        }
    };

    /** Applies all damage, and destroys each entity that reaches 0 health. */
    class DamageSystem final
        : public System<DamageSystem,
                        Access<Consume<DamageRequests>, Read<Invulnerable>, Write<Health>, Write<DamageStats>>>
    {
    public:
        static constexpr SystemInfo Info{.Name = "Tutorial.Damage", .Phase = Phases::Simulation};

        // No OnCreate: the engine makes DamageRequests (this system consumes it) and DamageStats
        // (this system writes it), and removes them when the world shuts down

        // Each play starts with no kills
        void OnPlayBegin(Context& ctx) const { ctx.Singleton<DamageStats>() = DamageStats{}; }

        void FixedUpdate(Context& ctx) const
        {
            DamageStats& stats = ctx.Singleton<DamageStats>();
            for (const DamageRequest& request : ctx.Drain<DamageRequests>())
            {
                // A request can point at an entity that is gone, so follow it with TryGet
                Health* health = ctx.TryGet<Health>(request.Target);
                if (health == nullptr || health->Current <= 0.0f || ctx.Has<Invulnerable>(request.Target))
                {
                    continue;
                }

                health->Current -= request.Amount;
                if (health->Current <= 0.0f)
                {
                    // Destroy waits until the end of the tick
                    ctx.Commands().Destroy(request.Target);
                    ++stats.Kills;
                }
            }
        }
    };

    // --- Step 3: registration -----------------------------------------------------
    // In a game this is Game::Register (or Register<Module> in an engine module).

    void RegisterTutorial(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<Health>(
            [](TypeBuilder<Health>& t_Type)
            {
                t_Type.Category("Combat");
                t_Type.Field("current", &Health::Current).Range(0.0f, 1000.0f);
                t_Type.Field("max", &Health::Max).Range(1.0f, 1000.0f).Tooltip("Full health");
            });
        t_Modules.Component<Poison>([](TypeBuilder<Poison>& t_Type)
                                    { t_Type.Field("damagePerSecond", &Poison::DamagePerSecond); });
        t_Modules.Component<Invulnerable>();
        t_Modules.Component<DamageRequests>();
        t_Modules.Component<DamageStats>();

        t_Modules.System<PoisonSystem>();
        t_Modules.System<DamageSystem>();
    }

    // --- Step 4: a test -----------------------------------------------------------

    /** Registers the tutorial module, and runs its systems on an EntityStorage. */
    struct TutorialWorld
    {
        EntityStorage Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        TutorialWorld()
        {
            ModuleRegistration modules(Systems);
            RegisterTutorial(modules);

            ScheduleResult schedule = Systems.BuildSchedule();
            REQUIRE(schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(Systems, std::move(schedule.Value), Storage);
            Runner->Create();
        }

        ~TutorialWorld() { Runner->Destroy(); }

        Entity MakeTarget(const float health, const float damagePerSecond)
        {
            const Entity e = Storage.Create();
            Storage.Add<Health>(e, Health{health, health});
            Storage.Add<Poison>(e, Poison{damagePerSecond});
            return e;
        }
    };
} // namespace

TEST_CASE("Tutorial: poison takes health each tick", "[ecs][tutorial]")
{
    TutorialWorld world;
    const Entity target = world.MakeTarget(100.0f, 60.0f); // 1 point per tick at 60 Hz

    world.Runner->RunTick(WorldRole::Play);
    world.Runner->RunTick(WorldRole::Play);

    REQUIRE(world.Storage.Get<Health>(target).Current == Catch::Approx(98.0f));
}

TEST_CASE("Tutorial: an entity at 0 health is destroyed and counted", "[ecs][tutorial]")
{
    TutorialWorld world;
    const Entity weak = world.MakeTarget(1.5f, 60.0f);
    const Entity strong = world.MakeTarget(100.0f, 60.0f);

    world.Runner->RunTick(WorldRole::Play);
    REQUIRE(world.Storage.IsValid(weak));

    world.Runner->RunTick(WorldRole::Play);
    REQUIRE_FALSE(world.Storage.IsValid(weak));
    REQUIRE(world.Storage.IsValid(strong));
    REQUIRE(world.Storage.Singleton<DamageStats>().Kills == 1);

    // A new play starts the count again
    world.Runner->PlayBegin(WorldRole::Play);
    REQUIRE(world.Storage.Singleton<DamageStats>().Kills == 0);
}

TEST_CASE("Tutorial: a tag stops damage", "[ecs][tutorial]")
{
    TutorialWorld world;
    const Entity target = world.MakeTarget(1.0f, 600.0f);
    world.Storage.Add<Invulnerable>(target);

    world.Runner->RunTick(WorldRole::Play);

    REQUIRE(world.Storage.IsValid(target));
    REQUIRE(world.Storage.Get<Health>(target).Current == 1.0f);
}

TEST_CASE("Tutorial: the schedule runs the appender before the consumer", "[ecs][tutorial]")
{
    TutorialWorld world;
    const std::vector<uint32> order = world.Runner->GetSchedule().RunOrder(UpdateRate::Fixed);

    std::vector<StringView> names;
    for (const uint32 index : order)
    {
        names.push_back(world.Systems.At(index).Describe().Info.Name);
    }
    REQUIRE(names == std::vector<StringView>{"Tutorial.Poison", "Tutorial.Damage"});
}

TEST_CASE("Tutorial: Health saves and loads by its field names", "[ecs][tutorial]")
{
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterTutorial(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);
    const TypeDesc& type = *types.Find<Health>();
    REQUIRE(type.Category == "Combat");

    const Health source{40.0f, 80.0f};
    Json saved = Json::Object();
    SaveObject(type, &source, saved, {});
    REQUIRE(saved.GetFloat("current") == 40.0f);
    REQUIRE(saved.GetFloat("max") == 80.0f);

    Health loaded;
    LoadObject(type, &loaded, Json::Parse(saved.Dump()), {});
    REQUIRE(loaded.Current == 40.0f);
    REQUIRE(loaded.Max == 80.0f);
}
