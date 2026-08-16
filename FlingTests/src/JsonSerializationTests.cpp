#include <catch2/catch_all.hpp>

#include "pch.h"
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

namespace
{
	class JsonTestGame : public Fling::Game
	{
	public:
		void Init(entt::registry&) override {}
		void OnStartGame(entt::registry&) override {}
		void Update(entt::registry&, float) override {}
		void OnStopGame(entt::registry&) override {}
		void Shutdown(entt::registry&) override {}
	};

	struct Health
	{
		int Amount = 0;
		float Armor = 0.f;

		void Serialize(Fling::JsonArchive& Ar)
		{
			Ar << Fling::MakeNVP("Amount", Amount);
			Ar << Fling::MakeNVP("Armor", Armor);
		}
	};

	void InitLogger()
	{
		Fling::Logger::Get().Init();
	}

	std::string TestLevelRelativePath()
	{
		return "Levels/_json_serialization_test.json";
	}

	std::string TestLevelFullPath()
	{
		return Fling::FlingPaths::EngineAssetsDir() + "/" + TestLevelRelativePath();
	}

	void RemoveTestLevelFile()
	{
		std::remove(TestLevelFullPath().c_str());
	}
}

TEST_CASE("Fling::Json parse dump Get Set", "[json]")
{
	using namespace Fling;
	InitLogger();

	SECTION("Parse and dump round-trip")
	{
		Json doc = Json::Parse(R"({"title":"Demo","count":3})");
		REQUIRE(doc.IsObject());
		REQUIRE(doc.Contains("title"));
		REQUIRE(doc.GetString("title") == "Demo");
		REQUIRE(doc.GetInt("count") == 3);

		Json parsedAgain = Json::Parse(doc.Dump(4));
		REQUIRE(parsedAgain.GetString("title") == "Demo");
		REQUIRE(parsedAgain.GetInt("count") == 3);
	}

	SECTION("Missing keys return defaults and do not insert")
	{
		Json doc = Json::Object();
		REQUIRE_FALSE(doc.Contains("missing"));
		REQUIRE(doc.GetString("missing", "fallback") == "fallback");
		REQUIRE(doc.GetFloat("missing", 4.5f) == Catch::Approx(4.5f));
		REQUIRE(doc.GetBool("missing", true) == true);
		REQUIRE(doc.GetInt("missing", 9) == 9);
		REQUIRE(doc.Get("missing").IsNull());
		REQUIRE_FALSE(doc.Contains("missing"));
	}

	SECTION("Get Set glm vec round-trip")
	{
		Json doc = Json::Object();
		doc.Set("position", glm::vec3(1.f, 2.f, 3.f));
		doc.Set("color", glm::vec4(0.1f, 0.2f, 0.3f, 0.4f));
		doc.Set("name", "hero");
		doc.Set("alive", true);
		doc.Set("speed", 12.5f);
		doc.Set("hp", 7);

		REQUIRE(doc.GetString("name") == "hero");
		REQUIRE(doc.GetBool("alive") == true);
		REQUIRE(doc.GetFloat("speed") == Catch::Approx(12.5f));
		REQUIRE(doc.GetInt("hp") == 7);

		const glm::vec3 pos = doc.GetVec3("position");
		REQUIRE(pos.x == Catch::Approx(1.f));
		REQUIRE(pos.y == Catch::Approx(2.f));
		REQUIRE(pos.z == Catch::Approx(3.f));

		const glm::vec4 color = doc.GetVec4("color");
		REQUIRE(color.x == Catch::Approx(0.1f));
		REQUIRE(color.y == Catch::Approx(0.2f));
		REQUIRE(color.z == Catch::Approx(0.3f));
		REQUIRE(color.w == Catch::Approx(0.4f));

		const glm::vec3 missing = doc.GetVec3("nope", glm::vec3(9.f, 8.f, 7.f));
		REQUIRE(missing.x == Catch::Approx(9.f));
	}

	SECTION("Array Get Set")
	{
		Json arr = Json::Array();
		Json a = Json::Object();
		a.Set("n", 1);
		Json b = Json::Object();
		b.Set("n", 2);
		arr.PushBack(a);
		arr.PushBack(b);

		REQUIRE(arr.IsArray());
		REQUIRE(arr.Size() == 2);
		REQUIRE(arr.At(0).GetInt("n") == 1);
		REQUIRE(arr.At(1).GetInt("n") == 2);
		REQUIRE(arr.At(5).IsNull());
	}
}

TEST_CASE("JsonArchive save and load round-trip", "[json]")
{
	using namespace Fling;
	InitLogger();

	struct Sample
	{
		float Speed = 0.f;
		int Count = 0;
		bool Flag = false;
		std::string Name;
		glm::vec3 Pos{};
		glm::vec4 Color{};

		void Serialize(JsonArchive& Ar)
		{
			Ar << MakeNVP("Speed", Speed);
			Ar << MakeNVP("Count", Count);
			Ar << MakeNVP("Flag", Flag);
			Ar << MakeNVP("Name", Name);
			Ar << MakeNVP("Pos", Pos);
			Ar << MakeNVP("Color", Color);
		}
	};

	SECTION("Round-trip")
	{
		Sample src;
		src.Speed = 3.5f;
		src.Count = 7;
		src.Flag = true;
		src.Name = "hello";
		src.Pos = glm::vec3(1.f, 2.f, 3.f);
		src.Color = glm::vec4(0.1f, 0.2f, 0.3f, 0.4f);

		Json doc = Json::Object();
		{
			JsonArchive ar(doc, JsonArchive::Mode::Saving);
			src.Serialize(ar);
		}

		Sample dst;
		{
			JsonArchive ar(doc, JsonArchive::Mode::Loading);
			dst.Serialize(ar);
		}

		REQUIRE(dst.Speed == Catch::Approx(3.5f));
		REQUIRE(dst.Count == 7);
		REQUIRE(dst.Flag == true);
		REQUIRE(dst.Name == "hello");
		REQUIRE(dst.Pos.x == Catch::Approx(1.f));
		REQUIRE(dst.Pos.y == Catch::Approx(2.f));
		REQUIRE(dst.Pos.z == Catch::Approx(3.f));
		REQUIRE(dst.Color.w == Catch::Approx(0.4f));
	}

	SECTION("Missing keys leave member defaults")
	{
		Json empty = Json::Object();
		Sample dst;
		dst.Speed = 99.f;
		dst.Name = "keep";

		JsonArchive ar(empty, JsonArchive::Mode::Loading);
		dst.Serialize(ar);

		REQUIRE(dst.Speed == Catch::Approx(99.f));
		REQUIRE(dst.Name == "keep");
	}
}

TEST_CASE("ComponentTypeRegistry hybrid register and seal", "[json]")
{
	using namespace Fling;
	InitLogger();

	ComponentTypeRegistry::Get().ResetForTests();

	SECTION("Register save load via type info")
	{
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

	SECTION("Register after Seal is rejected")
	{
		REQUIRE(ComponentTypeRegistry::Get().Register<Health>("Health"));
		ComponentTypeRegistry::Get().Seal();
		REQUIRE_FALSE(ComponentTypeRegistry::Get().Register<Health>("OtherHealth"));
		REQUIRE(ComponentTypeRegistry::Get().Find("OtherHealth") == nullptr);
	}

	SECTION("LoadLevelFile before Seal fails")
	{
		entt::registry reg;
		JsonTestGame game;
		World world(reg, &game);
		REQUIRE_FALSE(ComponentTypeRegistry::Get().IsSealed());
		REQUIRE_FALSE(world.LoadLevelFile("Levels/EmptyLevel.json"));
	}

	SECTION("World save load Transform and skip unknown keys")
	{
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
		reg.view<Transform, NameComponent>().each([&](auto /*entity*/, Transform& t, NameComponent& name)
		{
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

	ComponentTypeRegistry::Get().ResetForTests();
}
