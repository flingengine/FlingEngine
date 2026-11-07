#include "Components/EntityGuid.h"
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
#include "World.h"

#include <catch2/catch_all.hpp>
#include <functional>
#include <string>

using namespace Fling;

namespace
{
    void QuietLogger()
    {
        Logger::Get().Init();
        Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }

    enum class Team : int8
    {
        Red = -1,
        Blue = 2,
        Green = 7,
    };

    struct AllKinds
    {
        static constexpr ComponentInfo Component{.Name = "Test.AllKinds"};
        bool Flag = false;
        int32 Small = 0;
        uint32 Count = 0;
        int64 Wide = 0;
        uint64 Id = 0;
        float Speed = 0.0f;
        double Precise = 0.0;
        String Label;
        glm::vec2 Uv{0.0f};
        glm::vec3 Pos{0.0f};
        glm::vec4 Color{0.0f};
        glm::quat Spin{1.0f, 0.0f, 0.0f, 0.0f};
        Team Side = Team::Blue;
        Entity Target = Entity::Null;
    };

    /** Version 2 stores the speed in meters per second. Version 1 stored centimeters. */
    struct Runner
    {
        static constexpr ComponentInfo Component{.Name = "Test.Runner"};
        float Speed = 1.0f;
        float Stamina = 10.0f;
    };

    /** A component that refers to another entity. */
    struct Follow
    {
        static constexpr ComponentInfo Component{.Name = "ReflectionTest.Follow"};
        Entity Leader = Entity::Null;
        float Distance = 2.0f;
    };

    struct Marker
    {
        static constexpr ComponentInfo Component{.Name = "Test.Marker"};
        int Value = 0;
    };

    void RegisterTestTypes(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<AllKinds>(
            [](TypeBuilder<AllKinds>& t_Type)
            {
                t_Type.Field("flag", &AllKinds::Flag);
                t_Type.Field("small", &AllKinds::Small);
                t_Type.Field("count", &AllKinds::Count);
                t_Type.Field("wide", &AllKinds::Wide);
                t_Type.Field("id", &AllKinds::Id);
                t_Type.Field("speed", &AllKinds::Speed);
                t_Type.Field("precise", &AllKinds::Precise);
                t_Type.Field("label", &AllKinds::Label);
                t_Type.Field("uv", &AllKinds::Uv);
                t_Type.Field("pos", &AllKinds::Pos);
                t_Type.Field("color", &AllKinds::Color).Color();
                t_Type.Field("spin", &AllKinds::Spin);
                t_Type.Field("side", &AllKinds::Side)
                    .Enum<Team>({{"red", Team::Red}, {"blue", Team::Blue}, {"green", Team::Green}});
                t_Type.Field("target", &AllKinds::Target);
            });

        t_Modules.Component<Runner>(
            [](TypeBuilder<Runner>& t_Type)
            {
                t_Type.Version(2);
                t_Type.Field("speed", &Runner::Speed).FormerNames({"velocity"});
                t_Type.Field("stamina", &Runner::Stamina);
                t_Type.Removed("legacyFlag");
                t_Type.Upgrade(1,
                               [](Json& t_Data)
                               {
                                   // Centimeters to meters. Old files may use the former name.
                                   const char* key = t_Data.Contains("speed") ? "speed" : "velocity";
                                   t_Data.Set(key, t_Data.GetFloat(key) / 100.0f);
                               });
            });

        t_Modules.Component<Follow>(
            [](TypeBuilder<Follow>& t_Type)
            {
                t_Type.Field("leader", &Follow::Leader);
                t_Type.Field("distance", &Follow::Distance);
            });

        t_Modules.Component<Marker>(
            [](TypeBuilder<Marker>& t_Type)
            {
                t_Type.FormerNames({"Test.OldMarker"});
                t_Type.Field("value", &Marker::Value);
            });

        // A deleted component that turns into a Marker
        t_Modules.RemovedComponent("Test.Legacy",
                                   [](EntityStorage& t_Storage, const Entity t_Entity, const Json& t_Data)
                                   { t_Storage.AddOrReplace<Marker>(t_Entity, Marker{t_Data.GetInt("n") * 10}); });
        t_Modules.RemovedComponent("Test.Gone");
    }

    TypeRegistry BuildTestTypes(SystemRegistry& t_Systems, ModuleRegistration& t_Modules)
    {
        (void)t_Systems;
        RegisterTestTypes(t_Modules);
        return TypeRegistry::Build(t_Modules);
    }

    class ReflectionGame : public Game
    {
    public:
        void Init(EntityStorage&) override {}
        void Register(ModuleRegistration& t_Modules) override
        {
            RegisterEngine(t_Modules);
            RegisterTestTypes(t_Modules);
        }
        void OnStartGame(EntityStorage&) override {}
        void OnStopGame(EntityStorage&) override {}
        void Shutdown(EntityStorage&) override {}
    };
} // namespace

TEST_CASE("Every field kind saves and loads", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    const TypeRegistry types = BuildTestTypes(systems, modules);
    const TypeDesc& type = *types.Find<AllKinds>();
    REQUIRE(type.Fields.size() == 14);

    AllKinds source;
    source.Flag = true;
    source.Small = -12;
    source.Count = 4000000000u;
    source.Wide = -9000000000ll;
    source.Id = 0xFFFFFFFFFFFFFFF0ull;
    source.Speed = 2.5f;
    source.Precise = 0.123456789012345;
    source.Label = "hello";
    source.Uv = glm::vec2(0.25f, 0.5f);
    source.Pos = glm::vec3(1.0f, 2.0f, 3.0f);
    source.Color = glm::vec4(0.1f, 0.2f, 0.3f, 0.4f);
    source.Spin = glm::quat(0.5f, 0.5f, 0.5f, 0.5f);
    source.Side = Team::Red;

    Json saved = Json::Object();
    SaveObject(type, &source, saved, {});
    REQUIRE(saved.GetString("side") == "red");
    REQUIRE_FALSE(saved.Contains("$v"));

    // Through text, as a level file does
    const Json parsed = Json::Parse(saved.Dump());
    AllKinds loaded;
    LoadObject(type, &loaded, parsed, {});

    REQUIRE(loaded.Flag);
    REQUIRE(loaded.Small == -12);
    REQUIRE(loaded.Count == 4000000000u);
    REQUIRE(loaded.Wide == -9000000000ll);
    REQUIRE(loaded.Id == 0xFFFFFFFFFFFFFFF0ull);
    REQUIRE(loaded.Speed == 2.5f);
    REQUIRE(loaded.Precise == 0.123456789012345);
    REQUIRE(loaded.Label == "hello");
    REQUIRE(loaded.Uv == source.Uv);
    REQUIRE(loaded.Pos == source.Pos);
    REQUIRE(loaded.Color == source.Color);
    REQUIRE(loaded.Spin == source.Spin);
    REQUIRE(loaded.Side == Team::Red);
    REQUIRE(loaded.Target.IsNull());
}

TEST_CASE("A missing field keeps its default and an unknown enum name is ignored", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    const TypeRegistry types = BuildTestTypes(systems, modules);

    Json data = Json::Object();
    data.Set("speed", 9.0f);
    data.Set("side", "purple");

    AllKinds loaded;
    loaded.Label = "kept";
    LoadObject(*types.Find<AllKinds>(), &loaded, data, {});
    REQUIRE(loaded.Speed == 9.0f);
    REQUIRE(loaded.Label == "kept");
    REQUIRE(loaded.Side == Team::Blue);

    // An enum saved as a number (no table entry) still loads
    data.SetInt64("side", 7);
    LoadObject(*types.Find<AllKinds>(), &loaded, data, {});
    REQUIRE(loaded.Side == Team::Green);
}

TEST_CASE("Versioning: upgrades, former names, and removed fields", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    const TypeRegistry types = BuildTestTypes(systems, modules);
    const TypeDesc& type = *types.Find<Runner>();

    SECTION("A file with no $v is version 1, and the upgrade runs")
    {
        const Json old = Json::Parse(R"({ "speed": 250.0, "stamina": 3.0, "legacyFlag": true })");
        Runner loaded;
        LoadObject(type, &loaded, old, {});
        REQUIRE(loaded.Speed == Catch::Approx(2.5f));
        REQUIRE(loaded.Stamina == 3.0f);
    }

    SECTION("The upgrade and the former name work together")
    {
        const Json old = Json::Parse(R"({ "velocity": 400.0 })");
        Runner loaded;
        LoadObject(type, &loaded, old, {});
        REQUIRE(loaded.Speed == Catch::Approx(4.0f));
        REQUIRE(loaded.Stamina == 10.0f);
    }

    SECTION("A current file saves $v and loads without the upgrade")
    {
        Runner source;
        source.Speed = 7.0f;
        Json saved = Json::Object();
        SaveObject(type, &source, saved, {});
        REQUIRE(saved.GetInt64("$v") == 2);
        REQUIRE(saved.Contains("speed"));
        REQUIRE_FALSE(saved.Contains("velocity"));

        Runner loaded;
        LoadObject(type, &loaded, saved, {});
        REQUIRE(loaded.Speed == 7.0f);
    }

    SECTION("A component's former name finds the type")
    {
        REQUIRE(types.Find("Test.OldMarker") == types.Find<Marker>());
        REQUIRE(types.Find("Test.OldMarker")->Name == "Test.Marker");
    }
}

TEST_CASE("The world saves entity references and loads them into the new entities", "[reflection][world]")
{
    QuietLogger();
    const String LevelPath = "Levels/_reflection_test.json";
    const String FullPath = FlingPaths::EngineAssetsDir() + "/" + LevelPath;

    Json levelData;
    {
        ReflectionGame game;
        World world(&game);
        REQUIRE(world.Init());
        EntityStorage& storage = world.GetStorage();

        const Entity leader = storage.Create();
        storage.Add<NameComponent>(leader, NameComponent{Name("Leader")});
        const Entity follower = storage.Create();
        storage.Add<NameComponent>(follower, NameComponent{Name("Follower")});
        storage.Add<Follow>(follower, Follow{leader, 3.0f});
        const Entity loner = storage.Create();
        storage.Add<NameComponent>(loner, NameComponent{Name("Loner")});
        storage.Add<Follow>(loner, Follow{Entity::Null, 1.0f});

        REQUIRE(world.OutputLevelFile(LevelPath));
        REQUIRE(storage.Has<EntityGuid>(leader));
        REQUIRE(storage.Get<EntityGuid>(leader).Value != 0);
        REQUIRE(levelData.LoadFromFile(FullPath));
        world.Shutdown();
    }

    // Load into a new world. The entities have new handles, and the reference follows.
    ReflectionGame game;
    World world(&game);
    REQUIRE(world.Init());
    REQUIRE(world.LoadLevelFile(LevelPath));
    EntityStorage& storage = world.GetStorage();

    Entity leader = Entity::Null;
    Entity follower = Entity::Null;
    Entity loner = Entity::Null;
    storage.View<NameComponent>().Each(
        [&](const Entity e, NameComponent& name)
        {
            if (name.Name == "Leader"_name)
            {
                leader = e;
            }
            else if (name.Name == "Follower"_name)
            {
                follower = e;
            }
            else if (name.Name == "Loner"_name)
            {
                loner = e;
            }
        });
    REQUIRE_FALSE(leader.IsNull());
    REQUIRE(storage.Get<Follow>(follower).Leader == leader);
    REQUIRE(storage.Get<Follow>(follower).Distance == 3.0f);
    REQUIRE(storage.Get<Follow>(loner).Leader.IsNull());

    // A second save gives the same ids, so the file does not change
    Json second;
    REQUIRE(world.OutputLevelFile(LevelPath));
    REQUIRE(second.LoadFromFile(FullPath));
    REQUIRE(second.Dump() == levelData.Dump());

    world.Shutdown();
    std::remove(FullPath.c_str());
}

TEST_CASE("Removed and renamed components in a level", "[reflection][world]")
{
    QuietLogger();
    const String LevelPath = "Levels/_reflection_removed_test.json";
    const String FullPath = FlingPaths::EngineAssetsDir() + "/" + LevelPath;

    const Json level = Json::Parse(R"({
        "version": 1,
        "title": "Removed",
        "entities": [
            { "name": "A", "Test.Legacy": { "n": 4 } },
            { "name": "B", "Test.Gone": { "x": 1 }, "Test.OldMarker": { "value": 5 } }
        ]
    })");
    REQUIRE(level.SaveToFile(FullPath));

    ReflectionGame game;
    World world(&game);
    REQUIRE(world.Init());
    REQUIRE(world.LoadLevelFile(LevelPath));
    EntityStorage& storage = world.GetStorage();

    int markers = 0;
    storage.View<NameComponent, Marker>().Each(
        [&](NameComponent& name, Marker& marker)
        {
            ++markers;
            if (name.Name == "A"_name)
            {
                REQUIRE(marker.Value == 40);
            }
            else
            {
                REQUIRE(marker.Value == 5);
            }
        });
    REQUIRE(markers == 2);

    world.Shutdown();
    std::remove(FullPath.c_str());
}

TEST_CASE("Level_1 loads, and saves the same components again", "[reflection][world]")
{
    QuietLogger();
    ReflectionGame game;
    World world(&game);
    REQUIRE(world.Init());

    Json original;
    REQUIRE(original.LoadFromFile(FlingPaths::EngineAssetsDir() + "/Levels/Level_1.json"));
    REQUIRE(world.LoadLevelFile("Levels/Level_1.json"));

    const String LevelPath = "Levels/_level1_roundtrip_test.json";
    const String FullPath = FlingPaths::EngineAssetsDir() + "/" + LevelPath;
    REQUIRE(world.OutputLevelFile(LevelPath));
    Json saved;
    REQUIRE(saved.LoadFromFile(FullPath));

    const Json before = original.Get("entities");
    const Json after = saved.Get("entities");
    REQUIRE(before.Size() == after.Size());

    // Each saved entity has the components it had in the file, plus an EntityGuid.
    // Graphics types are not registered in this test, so compare the Transforms.
    size_t transforms = 0;
    for (size_t i = 0; i < after.Size(); ++i)
    {
        const Json entity = after.At(i);
        REQUIRE(entity.Contains("EntityGuid"));
        if (entity.Contains("Transform"))
        {
            ++transforms;
        }
    }
    size_t originalTransforms = 0;
    for (size_t i = 0; i < before.Size(); ++i)
    {
        originalTransforms += before.At(i).Contains("Transform") ? 1 : 0;
    }
    REQUIRE(transforms == originalTransforms);

    world.Shutdown();
    std::remove(FullPath.c_str());
}

namespace
{
    /** A big padding gap: an 8-bit enum before a 16-byte aligned member, as Collider has with aligned GLM types. */
    struct PaddedShape
    {
        static constexpr ComponentInfo Component{.Name = "Test.PaddedShape"};
        Team Side = Team::Red;
        alignas(16) glm::vec4 Extents{0.0f};
        float Radius = 0.0f;
    };

    struct Forgotten
    {
        static constexpr ComponentInfo Component{.Name = "Test.Forgotten"};
        float Saved = 0.0f;
        double NotReflected = 0.0;
        int Last = 0;
    };
} // namespace

TEST_CASE("UnreflectedBytes tells padding from forgotten fields", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    modules.Component<PaddedShape>(
        [](TypeBuilder<PaddedShape>& t_Type)
        {
            t_Type.Field("side", &PaddedShape::Side);
            t_Type.Field("extents", &PaddedShape::Extents);
            t_Type.Field("radius", &PaddedShape::Radius);
        });
    modules.Component<Forgotten>(
        [](TypeBuilder<Forgotten>& t_Type)
        {
            t_Type.Field("saved", &Forgotten::Saved);
            t_Type.Field("last", &Forgotten::Last);
        });
    const TypeRegistry types = TypeRegistry::Build(modules);

    // 15 bytes after the enum and 12 at the end are padding, not forgotten fields
    STATIC_REQUIRE(sizeof(PaddedShape) == 48);
    REQUIRE(UnreflectedBytes(*types.Find<PaddedShape>()) == 0);

    // The 8-byte double between the reflected fields is not padding
    REQUIRE(UnreflectedBytes(*types.Find<Forgotten>()) >= sizeof(double));

    // AllKinds reflects every member, and its gaps are only padding
    SystemRegistry systems2;
    ModuleRegistration modules2(systems2);
    RegisterTestTypes(modules2);
    const TypeRegistry all = TypeRegistry::Build(modules2);
    REQUIRE(UnreflectedBytes(*all.Find<AllKinds>()) == 0);
    REQUIRE(UnreflectedBytes(*all.Find<Runner>()) == 0);
}

namespace
{
    enum class Wide16 : int16
    {
        Low = -300,
        High = 300,
    };

    enum class Wide32 : uint32
    {
        Big = 4000000000u,
    };

    enum class Wide64 : int64
    {
        Huge = -9000000000ll,
    };

    /** A field type with its own save and load (FieldTraits Kind = Custom). */
    struct Rgb
    {
        uint8 R = 0;
        uint8 G = 0;
        uint8 B = 0;
    };

    /** Enums of each size, a Name, and a custom field type. */
    struct EdgeKinds
    {
        static constexpr ComponentInfo Component{.Name = "Test.EdgeKinds"};
        Wide16 Small = Wide16::Low;
        Wide32 Medium = Wide32::Big;
        Wide64 Large = Wide64::Huge;
        Wide16 Untabled = Wide16::High;
        Name Label;
        Rgb Tint;
    };

    /** Saved as one string by TypeBuilder::Serialize, not field by field. */
    struct Packed
    {
        static constexpr ComponentInfo Component{.Name = "Test.Packed"};
        int32 A = 0;
        int32 B = 0;
    };

    /** Version 3, with an upgrade from 2 only: a version 1 file has a missing step. */
    struct Gapped
    {
        static constexpr ComponentInfo Component{.Name = "Test.Gapped"};
        float Value = 0.0f;
    };

    void RegisterEdgeTypes(ModuleRegistration& t_Modules)
    {
        t_Modules.Component<EdgeKinds>(
            [](TypeBuilder<EdgeKinds>& t_Type)
            {
                t_Type.Field("small", &EdgeKinds::Small).Enum<Wide16>({{"low", Wide16::Low}, {"high", Wide16::High}});
                t_Type.Field("medium", &EdgeKinds::Medium).Enum<Wide32>({{"big", Wide32::Big}});
                t_Type.Field("large", &EdgeKinds::Large).Enum<Wide64>({{"huge", Wide64::Huge}});
                t_Type.Field("untabled", &EdgeKinds::Untabled);
                t_Type.Field("label", &EdgeKinds::Label);
                t_Type.Field("tint", &EdgeKinds::Tint);
            });
        t_Modules.Component<Packed>(
            [](TypeBuilder<Packed>& t_Type)
            {
                t_Type.Serialize([](const Packed& t_Value, Json& t_Out)
                                 { t_Out.SetInt64("ab", (int64(t_Value.A) << 32) | int64(uint32(t_Value.B))); },
                                 [](Packed& t_Value, const Json& t_In)
                                 {
                                     const int64 packed = t_In.GetInt64("ab");
                                     t_Value.A = int32(packed >> 32);
                                     t_Value.B = int32(uint32(packed));
                                 });
            });
        t_Modules.Component<Gapped>(
            [](TypeBuilder<Gapped>& t_Type)
            {
                t_Type.Version(3);
                t_Type.Field("value", &Gapped::Value);
                t_Type.Upgrade(2, [](Json& t_Data) { t_Data.Set("value", t_Data.GetFloat("value") * 2.0f); });
            });
    }
} // namespace

template <>
struct Fling::FieldTraits<Rgb>
{
    static constexpr FieldKind Kind = FieldKind::Custom;

    static void Save(const Rgb& t_Value, Json& t_Object, const StringView t_Key)
    {
        t_Object.SetInt64(t_Key, (int64(t_Value.R) << 16) | (int64(t_Value.G) << 8) | int64(t_Value.B));
    }

    static bool Load(Rgb& t_Value, const Json& t_Object, const StringView t_Key)
    {
        if (!t_Object.Contains(t_Key))
        {
            return false;
        }
        const int64 packed = t_Object.GetInt64(t_Key);
        t_Value = Rgb{uint8(packed >> 16), uint8(packed >> 8), uint8(packed)};
        return true;
    }
};

TEST_CASE("Enums of every size, Names, and custom field types save and load", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterEdgeTypes(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);
    const TypeDesc& type = *types.Find<EdgeKinds>();

    EdgeKinds source;
    source.Small = Wide16::High;
    source.Label = Name("Tutorial.Label");
    source.Tint = Rgb{1, 2, 3};

    Json saved = Json::Object();
    SaveObject(type, &source, saved, {});
    REQUIRE(saved.GetString("small") == "high");
    REQUIRE(saved.GetString("medium") == "big");
    REQUIRE(saved.GetString("large") == "huge");
    REQUIRE(saved.GetInt64("untabled") == 300); // No table, so it saves as a number
    REQUIRE(saved.GetString("label") == "Tutorial.Label");
    REQUIRE(saved.GetInt64("tint") == 0x010203);

    EdgeKinds loaded;
    loaded.Medium = static_cast<Wide32>(0);
    loaded.Large = static_cast<Wide64>(0);
    loaded.Untabled = Wide16::Low;
    LoadObject(type, &loaded, Json::Parse(saved.Dump()), {});
    REQUIRE(loaded.Small == Wide16::High);
    REQUIRE(loaded.Medium == Wide32::Big);
    REQUIRE(loaded.Large == Wide64::Huge);
    REQUIRE(loaded.Untabled == Wide16::High);
    REQUIRE(loaded.Label == Name("Tutorial.Label"));
    REQUIRE(loaded.Tint.R == 1);
    REQUIRE(loaded.Tint.G == 2);
    REQUIRE(loaded.Tint.B == 3);

    // A custom field with no key keeps its value
    EdgeKinds kept;
    kept.Tint = Rgb{9, 9, 9};
    LoadObject(type, &kept, Json::Object(), {});
    REQUIRE(kept.Tint.R == 9);
}

TEST_CASE("A whole-type Serialize replaces the fields", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterEdgeTypes(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);
    const TypeDesc& type = *types.Find<Packed>();

    const Packed source{4, -5};
    Json saved = Json::Object();
    SaveObject(type, &source, saved, {});
    REQUIRE(saved.Contains("ab"));
    REQUIRE_FALSE(saved.Contains("a"));

    Packed loaded;
    LoadObject(type, &loaded, saved, {});
    REQUIRE(loaded.A == 4);
    REQUIRE(loaded.B == -5);
}

TEST_CASE("A newer saved version, or a missing upgrade step, still loads fields by name", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    RegisterEdgeTypes(modules);
    const TypeRegistry types = TypeRegistry::Build(modules);
    const TypeDesc& type = *types.Find<Gapped>();

    // Saved by a newer build: no upgrade runs
    Json newer = Json::Object();
    newer.SetInt64("$v", 9);
    newer.Set("value", 1.5f);
    Gapped fromNewer;
    LoadObject(type, &fromNewer, newer, {});
    REQUIRE(fromNewer.Value == 1.5f);

    // Version 1 has no step to 2, so only the 2 -> 3 step runs
    Json old = Json::Object();
    old.Set("value", 1.5f);
    Gapped fromOld;
    LoadObject(type, &fromOld, old, {});
    REQUIRE(fromOld.Value == 3.0f);

    // Version 3 saves its version
    Json saved = Json::Object();
    SaveObject(type, &fromOld, saved, {});
    REQUIRE(saved.GetInt64("$v") == 3);
}

namespace
{
    namespace One
    {
        struct Clash
        {
            static constexpr ComponentInfo Component{.Name = "Test.Clash"};
            float A = 0.0f;
        };
    } // namespace One

    namespace Two
    {
        struct Clash
        {
            static constexpr ComponentInfo Component{.Name = "Test.Clash"};
            float A = 0.0f;
        };
    } // namespace Two

    struct Renamed
    {
        static constexpr ComponentInfo Component{.Name = "Test.Renamed"};
        float A = 0.0f;
    };

    struct AlsoRenamed
    {
        static constexpr ComponentInfo Component{.Name = "Test.AlsoRenamed"};
        float A = 0.0f;
    };

    bool HasError(const TypeRegistry& t_Types, const StringView t_Text)
    {
        for (const String& error : t_Types.Errors())
        {
            if (error.find(t_Text) != String::npos)
            {
                return true;
            }
        }
        return false;
    }
} // namespace

TEST_CASE("Two C++ types with one saved name are an error", "[reflection]")
{
    QuietLogger();
    SystemRegistry systems;
    ModuleRegistration modules(systems);
    modules.Component<One::Clash>();
    modules.Component<One::Clash>(); // The same type again is only a warning
    REQUIRE(modules.Errors().empty());

    modules.Component<Two::Clash>();
    const TypeRegistry types = TypeRegistry::Build(modules);
    REQUIRE_FALSE(types.Ok());
    REQUIRE(HasError(types, "both have the saved name 'Test.Clash'"));
    REQUIRE(HasError(types, "One::Clash"));
    REQUIRE(HasError(types, "Two::Clash"));
}

TEST_CASE("A former name must name only one type", "[reflection]")
{
    QuietLogger();

    SECTION("A clean rename is not an error, and the old name finds the type")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Renamed>([](TypeBuilder<Renamed>& t_Type) { t_Type.FormerNames({"Test.OldName"}); });
        const TypeRegistry types = TypeRegistry::Build(modules);
        REQUIRE(types.Ok());
        REQUIRE(types.Find("Test.OldName") == types.Find<Renamed>());
    }

    SECTION("Another type's saved name")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<AlsoRenamed>();
        modules.Component<Renamed>([](TypeBuilder<Renamed>& t_Type) { t_Type.FormerNames({"Test.AlsoRenamed"}); });
        const TypeRegistry types = TypeRegistry::Build(modules);
        REQUIRE(HasError(types, "'Test.AlsoRenamed' is a former name of"));
        REQUIRE(types.Find("Test.AlsoRenamed") == types.Find<AlsoRenamed>());
    }

    SECTION("Two types claim one former name")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Renamed>([](TypeBuilder<Renamed>& t_Type) { t_Type.FormerNames({"Test.Old"}); });
        modules.Component<AlsoRenamed>([](TypeBuilder<AlsoRenamed>& t_Type) { t_Type.FormerNames({"Test.Old"}); });
        REQUIRE(HasError(TypeRegistry::Build(modules), "'Test.Old' is a former name of both"));
    }

    SECTION("The type's own name")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Renamed>([](TypeBuilder<Renamed>& t_Type) { t_Type.FormerNames({"Test.Renamed"}); });
        REQUIRE(HasError(TypeRegistry::Build(modules), "lists its own name"));
    }

    SECTION("A removed component's name")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Renamed>([](TypeBuilder<Renamed>& t_Type) { t_Type.FormerNames({"Test.Gone"}); });
        modules.RemovedComponent("Test.Gone");
        REQUIRE(HasError(TypeRegistry::Build(modules), "'Test.Gone' is a former name of"));
    }

    SECTION("A registered component that is also removed")
    {
        SystemRegistry systems;
        ModuleRegistration modules(systems);
        modules.Component<Renamed>();
        modules.RemovedComponent("Test.Renamed");
        REQUIRE(HasError(TypeRegistry::Build(modules), "also a RemovedComponent"));
    }
}
