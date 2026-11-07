#include "Components/Name.hpp"
#include "Components/Transform.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/RegisterEngine.h"
#include "Ecs/TypeRegistry.h"
#include "FlingPaths.h"
#include "Game.h"
#include "Json.h"
#include "Logger.h"
#include "Strings/String.h"
#include "TransformUtil.h"
#include "World.h"

#include <catch2/catch_all.hpp>
#include <cstdio>
#include <functional>

using Fling::String;
using Fling::StringView;

namespace
{
    class JsonTestGame : public Fling::Game
    {
    public:
        void Init(Fling::EntityStorage&) override {}
        void Register(Fling::ModuleRegistration& t_Modules) override { Fling::RegisterEngine(t_Modules); }
        void OnStartGame(Fling::EntityStorage&) override {}
        void Update(Fling::EntityStorage&, float) override {}
        void OnStopGame(Fling::EntityStorage&) override {}
        void Shutdown(Fling::EntityStorage&) override {}
    };

    struct Health
    {
        static constexpr Fling::ComponentInfo Component{.Name = "Health"};
        int Amount = 0;
        float Armor = 0.f;
    };

    void InitLogger()
    {
        Fling::Logger::Get().Init();
        // These cases assert expected failure paths (unsealed registry, unknown
        // component keys). Keep that off the console so the suite does not look failed.
        Fling::Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Fling::Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    String TestLevelRelativePath()
    {
        return "Levels/_json_serialization_test.json";
    }

    String TestLevelFullPath()
    {
        return Fling::FlingPaths::EngineAssetsDir() + "/" + TestLevelRelativePath();
    }

    void RemoveTestLevelFile()
    {
        std::remove(TestLevelFullPath().c_str());
    }
} // namespace

TEST_CASE("Reflected components save and load, and the world loads levels", "[json]")
{
    using namespace Fling;
    InitLogger();

    SECTION("Save and load through the type description")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Health>(
            [](TypeBuilder<Health>& t_Type)
            {
                t_Type.Field("Amount", &Health::Amount);
                t_Type.Field("Armor", &Health::Armor);
            });
        const TypeRegistry types = TypeRegistry::Build(modules);

        Fling::EntityStorage reg;
        const Fling::Entity entity = reg.Create();
        Health& src = reg.Add<Health>(entity);
        src.Amount = 42;
        src.Armor = 3.5f;

        const TypeDesc* info = types.Find("Health");
        REQUIRE(info != nullptr);
        REQUIRE(info->Has(reg, entity));

        Json out = Json::Object();
        SaveObject(*info, info->GetConst(reg, entity), out, {});
        REQUIRE(out.GetInt("Amount") == 42);
        REQUIRE(out.GetFloat("Armor") == Catch::Approx(3.5f));

        Fling::EntityStorage loaded;
        const Fling::Entity loadedEntity = loaded.Create();
        const std::function<void(void*)> fill = [&](void* t_Object)
        {
            LoadObject(*info, t_Object, out, {});
        };
        info->Emplace(loaded, loadedEntity, &fill);
        REQUIRE(loaded.Has<Health>(loadedEntity));
        REQUIRE(loaded.Get<Health>(loadedEntity).Amount == 42);
        REQUIRE(loaded.Get<Health>(loadedEntity).Armor == Catch::Approx(3.5f));
    }

    SECTION("LoadLevelFile before Init fails")
    {
        JsonTestGame game;
        World world(&game);
        REQUIRE(world.GetTypes().Empty());
        REQUIRE_FALSE(world.LoadLevelFile("Levels/EmptyLevel.json"));
    }

    SECTION("World save load Transform and skip unknown keys")
    {
        Json root = Json::Object();
        root.Set("version", 1);
        root.Set("title", "Unknown Key Level");

        Json entity = Json::Object();
        entity.Set("name", "Hero");

        Json transform = Json::Object();
        transform.Set("position", glm::vec3(4.f, 5.f, 6.f));
        transform.Set("rotation", glm::vec3(0.f, 90.f, 0.f));
        transform.Set("scale", glm::vec3(1.f, 1.f, 1.f));
        entity.Set("Transform", transform);

        Json mystery = Json::Object();
        mystery.Set("hp", 5);
        entity.Set("DoesNotExist", mystery);

        Json entities = Json::Array();
        entities.PushBack(entity);
        root.Set("entities", entities);

        RemoveTestLevelFile();
        REQUIRE(root.SaveToFile(TestLevelFullPath()));

        JsonTestGame game;
        World world(&game);
        Fling::EntityStorage& reg = world.GetStorage();
        REQUIRE(world.Init());
        REQUIRE(world.LoadLevelFile(TestLevelRelativePath()));

        int count = 0;
        reg.View<Transform, NameComponent>().Each(
            [&](auto /*entity*/, Transform& t, NameComponent& name)
            {
                ++count;
                REQUIRE(name.Name == "Hero"_name);
                REQUIRE(t.Position.x == Catch::Approx(4.f));
                REQUIRE(t.Position.y == Catch::Approx(5.f));
                REQUIRE(t.Position.z == Catch::Approx(6.f));
                REQUIRE(Fling::TransformUtil::ToEulerDegrees(t.Rotation).y == Catch::Approx(90.f));
            });
        REQUIRE(count == 1);

        RemoveTestLevelFile();
        world.Shutdown();
    }

    SECTION("World loads distinct Transform positions from one file")
    {
        auto MakeEntity = [](const char* name, const glm::vec3& pos)
        {
            Json entity = Json::Object();
            entity.Set("name", name);
            Json transform = Json::Object();
            transform.Set("position", pos);
            transform.Set("rotation", glm::vec3(0.f));
            transform.Set("scale", glm::vec3(1.f));
            entity.Set("Transform", transform);
            return entity;
        };

        Json root = Json::Object();
        root.Set("version", 1);
        root.Set("title", "Two Entities");
        Json entities = Json::Array();
        entities.PushBack(MakeEntity("Left", glm::vec3(-8.f, 1.f, 0.f)));
        entities.PushBack(MakeEntity("Right", glm::vec3(8.f, 2.f, 3.f)));
        root.Set("entities", entities);

        RemoveTestLevelFile();
        REQUIRE(root.SaveToFile(TestLevelFullPath()));

        JsonTestGame game;
        World world(&game);
        Fling::EntityStorage& reg = world.GetStorage();
        REQUIRE(world.Init());
        REQUIRE(world.LoadLevelFile(TestLevelRelativePath()));

        glm::vec3 left{};
        glm::vec3 right{};
        reg.View<Transform, NameComponent>().Each(
            [&](auto /*entity*/, Transform& t, NameComponent& name)
            {
                if (name.Name == "Left"_name)
                {
                    left = t.Position;
                }
                else if (name.Name == "Right"_name)
                {
                    right = t.Position;
                }
            });
        REQUIRE(left.x == Catch::Approx(-8.f));
        REQUIRE(left.y == Catch::Approx(1.f));
        REQUIRE(right.x == Catch::Approx(8.f));
        REQUIRE(right.y == Catch::Approx(2.f));
        REQUIRE(right.z == Catch::Approx(3.f));

        RemoveTestLevelFile();
        world.Shutdown();
    }
}

namespace
{
    struct DummyMesh
    {
        int Id = 0;
    };

    void SeedMeshEntities(Fling::EntityStorage& reg, int count)
    {
        for (int i = 0; i < count; ++i)
        {
            const Fling::Entity entity = reg.Create();
            Fling::Transform t;
            t.Position = glm::vec3(static_cast<float>(i), 0.f, 0.f);
            reg.Add<Fling::Transform>(entity, t);
            reg.Add<DummyMesh>(entity);
        }
    }

    void SeedTransformOnly(Fling::EntityStorage& reg)
    {
        const Fling::Entity entity = reg.Create();
        Fling::Transform t;
        t.Position = glm::vec3(0.f, 10.f, 0.f);
        reg.Add<Fling::Transform>(entity, t);
    }

    glm::vec3 SpawnMeshThenSetPos(Fling::EntityStorage& reg)
    {
        const Fling::Entity spawned = reg.Create();
        reg.Add<DummyMesh>(spawned);
        Fling::Transform& t = reg.Add<Fling::Transform>(spawned);
        t.Position = glm::vec3(99.f, 98.f, 97.f);
        return reg.Get<Fling::Transform>(spawned).Position;
    }
} // namespace

TEST_CASE("assign Transform reference stays valid with views", "[ecs]")
{
    Fling::EntityStorage reg;
    auto renderView = reg.View<Fling::Transform, DummyMesh>();
    (void)renderView;

    SeedMeshEntities(reg, 4);
    SeedTransformOnly(reg);

    const glm::vec3 pos = SpawnMeshThenSetPos(reg);
    REQUIRE(pos.x == Catch::Approx(99.f));
    REQUIRE(pos.y == Catch::Approx(98.f));
    REQUIRE(pos.z == Catch::Approx(97.f));
}
