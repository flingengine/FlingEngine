#include <catch2/catch_all.hpp>

#include "Json.h"
#include "JsonArchive.h"
#include "ComponentTypeRegistry.h"
#include "Components/Transform.h"
#include "Components/Name.hpp"
#include "FlingPaths.h"
#include "Game.h"
#include "World.h"

#include <cstdio>
#include <string>
#include "Logger.h"

namespace {
    class JsonTestGame : public Fling::Game {
    public:
        void Init(entt::registry&) override {}
        void OnStartGame(entt::registry&) override {}
        void Update(entt::registry&, float) override {}
        void OnStopGame(entt::registry&) override {}
        void Shutdown(entt::registry&) override {}
    };

    struct Health {
        int Amount = 0;
        float Armor = 0.f;

        void Serialize(Fling::JsonArchive& Ar) {
            Ar << Fling::MakeNVP("Amount", Amount);
            Ar << Fling::MakeNVP("Armor", Armor);
        }
    };

    void InitLogger() {
        Fling::Logger::Get().Init();
        // These cases assert expected failure paths (unsealed registry, unknown
        // component keys). Keep that off the console so the suite does not look failed.
        Fling::Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Fling::Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    std::string TestLevelRelativePath() {
        return "Levels/_json_serialization_test.json";
    }

    std::string TestLevelFullPath() {
        return Fling::FlingPaths::EngineAssetsDir() + "/" + TestLevelRelativePath();
    }

    void RemoveTestLevelFile() {
        std::remove(TestLevelFullPath().c_str());
    }
} // namespace

TEST_CASE("ComponentTypeRegistry hybrid register and seal", "[json]") {
    using namespace Fling;
    InitLogger();

    ComponentTypeRegistry::Get().ResetForTests();

    SECTION("Register save load via type info") {
        REQUIRE(ComponentTypeRegistry::Get().Register<Health>("Health"));
        ComponentTypeRegistry::Get().Seal();
        REQUIRE(ComponentTypeRegistry::Get().IsSealed());

        entt::registry reg;
        const entt::entity entity = reg.create();
        Health& src = reg.assign<Health>(entity);
        src.Amount = 42;
        src.Armor = 3.5f;

        const ComponentTypeInfo* info = ComponentTypeRegistry::Get().Find("Health");
        REQUIRE(info != nullptr);
        REQUIRE(info->has(reg, entity));

        Json out = Json::Object();
        info->save(reg, entity, out);
        REQUIRE(out.GetInt("Amount") == 42);
        REQUIRE(out.GetFloat("Armor") == Catch::Approx(3.5f));

        entt::registry loaded;
        const entt::entity loadedEntity = loaded.create();
        info->load(loaded, loadedEntity, out);
        REQUIRE(loaded.has<Health>(loadedEntity));
        REQUIRE(loaded.get<Health>(loadedEntity).Amount == 42);
        REQUIRE(loaded.get<Health>(loadedEntity).Armor == Catch::Approx(3.5f));
    }

    SECTION("Register after Seal is rejected") {
        REQUIRE(ComponentTypeRegistry::Get().Register<Health>("Health"));
        ComponentTypeRegistry::Get().Seal();
        REQUIRE_FALSE(ComponentTypeRegistry::Get().Register<Health>("OtherHealth"));
        REQUIRE(ComponentTypeRegistry::Get().Find("OtherHealth") == nullptr);
    }

    SECTION("LoadLevelFile before Seal fails") {
        entt::registry reg;
        JsonTestGame game;
        World world(reg, &game);
        REQUIRE_FALSE(ComponentTypeRegistry::Get().IsSealed());
        REQUIRE_FALSE(world.LoadLevelFile("Levels/EmptyLevel.json"));
    }

    SECTION("World save load Transform and skip unknown keys") {
        REQUIRE(ComponentTypeRegistry::Get().Register<Transform>("Transform"));
        REQUIRE(ComponentTypeRegistry::Get().Register<NameComponent>("NameComponent"));
        ComponentTypeRegistry::Get().Seal();

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

        entt::registry reg;
        JsonTestGame game;
        World world(reg, &game);
        REQUIRE(world.LoadLevelFile(TestLevelRelativePath()));

        int count = 0;
        reg.view<Transform, NameComponent>().each([&](auto /*entity*/, Transform& t, NameComponent& name) {
            ++count;
            REQUIRE(name.Name == "Hero");
            REQUIRE(t.GetPos().x == Catch::Approx(4.f));
            REQUIRE(t.GetPos().y == Catch::Approx(5.f));
            REQUIRE(t.GetPos().z == Catch::Approx(6.f));
            REQUIRE(t.GetRotation().y == Catch::Approx(90.f));
        });
        REQUIRE(count == 1);

        RemoveTestLevelFile();
    }

    SECTION("World loads distinct Transform positions from one file") {
        REQUIRE(ComponentTypeRegistry::Get().Register<Transform>("Transform"));
        REQUIRE(ComponentTypeRegistry::Get().Register<NameComponent>("NameComponent"));
        ComponentTypeRegistry::Get().Seal();

        auto MakeEntity = [](const char* name, const glm::vec3& pos) {
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

        entt::registry reg;
        JsonTestGame game;
        World world(reg, &game);
        REQUIRE(world.LoadLevelFile(TestLevelRelativePath()));

        glm::vec3 left{};
        glm::vec3 right{};
        reg.view<Transform, NameComponent>().each([&](auto /*entity*/, Transform& t, NameComponent& name) {
            if (name.Name == "Left") {
                left = t.GetPos();
            } else if (name.Name == "Right") {
                right = t.GetPos();
            }
        });
        REQUIRE(left.x == Catch::Approx(-8.f));
        REQUIRE(left.y == Catch::Approx(1.f));
        REQUIRE(right.x == Catch::Approx(8.f));
        REQUIRE(right.y == Catch::Approx(2.f));
        REQUIRE(right.z == Catch::Approx(3.f));

        RemoveTestLevelFile();
    }

    ComponentTypeRegistry::Get().ResetForTests();
}

namespace {
    struct DummyMesh {
        int Id = 0;
    };

    void SeedMeshEntities(entt::registry& reg, int count) {
        for (int i = 0; i < count; ++i) {
            const entt::entity entity = reg.create();
            Fling::Transform t;
            t.SetPos(glm::vec3(static_cast<float>(i), 0.f, 0.f));
            reg.assign<Fling::Transform>(entity, t);
            reg.assign<DummyMesh>(entity);
        }
    }

    void SeedTransformOnly(entt::registry& reg) {
        const entt::entity entity = reg.create();
        Fling::Transform t;
        t.SetPos(glm::vec3(0.f, 10.f, 0.f));
        reg.assign<Fling::Transform>(entity, t);
    }

    glm::vec3 SpawnMeshThenSetPos(entt::registry& reg) {
        const entt::entity spawned = reg.create();
        reg.assign<DummyMesh>(spawned);
        Fling::Transform& t = reg.assign<Fling::Transform>(spawned);
        t.SetPos(glm::vec3(99.f, 98.f, 97.f));
        return reg.get<Fling::Transform>(spawned).GetPos();
    }
} // namespace

TEST_CASE("assign Transform reference stays valid with views", "[ecs]") {
    entt::registry reg;
    auto renderView = reg.view<Fling::Transform, DummyMesh>();
    (void)renderView;

    SeedMeshEntities(reg, 4);
    SeedTransformOnly(reg);

    const glm::vec3 pos = SpawnMeshThenSetPos(reg);
    REQUIRE(pos.x == Catch::Approx(99.f));
    REQUIRE(pos.y == Catch::Approx(98.f));
    REQUIRE(pos.z == Catch::Approx(97.f));
}

TEST_CASE("owning groups can invalidate assign Transform references", "[ecs]") {
    entt::registry reg;
    auto packed = reg.group<Fling::Transform>(entt::get<DummyMesh>);
    (void)packed;

    SeedMeshEntities(reg, 4);
    SeedTransformOnly(reg);

    const glm::vec3 pos = SpawnMeshThenSetPos(reg);
    REQUIRE(pos.x != Catch::Approx(99.f));
}
