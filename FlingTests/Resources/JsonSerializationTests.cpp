#include <catch2/catch_all.hpp>

#include "Json.h"
#include "JsonArchive.h"
#include "Logger.h"

#include <string>

namespace {
    void InitLogger() {
        Fling::Logger::Get().Init();
        // Keep startup log lines off the console so a passing run stays quiet.
        Fling::Logger::GetCurrentConsole()->set_level(spdlog::level::off);
        Fling::Logger::GetCurrentLogFile()->set_level(spdlog::level::off);
    }
} // namespace

TEST_CASE("Fling::Json parse dump Get Set", "[json]") {
    using namespace Fling;
    InitLogger();

    SECTION("Parse and dump round-trip") {
        Json doc = Json::Parse(R"({"title":"Demo","count":3})");
        REQUIRE(doc.IsObject());
        REQUIRE(doc.Contains("title"));
        REQUIRE(doc.GetString("title") == "Demo");
        REQUIRE(doc.GetInt("count") == 3);

        Json parsedAgain = Json::Parse(doc.Dump(4));
        REQUIRE(parsedAgain.GetString("title") == "Demo");
        REQUIRE(parsedAgain.GetInt("count") == 3);
    }

    SECTION("Missing keys return defaults and do not insert") {
        Json doc = Json::Object();
        REQUIRE_FALSE(doc.Contains("missing"));
        REQUIRE(doc.GetString("missing", "fallback") == "fallback");
        REQUIRE(doc.GetFloat("missing", 4.5f) == Catch::Approx(4.5f));
        REQUIRE(doc.GetBool("missing", true) == true);
        REQUIRE(doc.GetInt("missing", 9) == 9);
        REQUIRE(doc.Get("missing").IsNull());
        REQUIRE_FALSE(doc.Contains("missing"));
    }

    SECTION("Get Set glm vec round-trip") {
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

    SECTION("Array Get Set") {
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

TEST_CASE("JsonArchive save and load round-trip", "[json]") {
    using namespace Fling;
    InitLogger();

    struct Sample {
        float Speed = 0.f;
        int Count = 0;
        bool Flag = false;
        std::string Name;
        glm::vec3 Pos{};
        glm::vec4 Color{};

        void Serialize(JsonArchive& Ar) {
            Ar << MakeNVP("Speed", Speed);
            Ar << MakeNVP("Count", Count);
            Ar << MakeNVP("Flag", Flag);
            Ar << MakeNVP("Name", Name);
            Ar << MakeNVP("Pos", Pos);
            Ar << MakeNVP("Color", Color);
        }
    };

    SECTION("Round-trip") {
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

    SECTION("Missing keys leave member defaults") {
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
