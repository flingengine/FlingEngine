#include "Json.h"
#include "Logger.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>

using Fling::String;
using Fling::StringView;

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

TEST_CASE("Fling::Json wide numbers, vec2, quat, and Remove", "[json]") {
    using Fling::Json;

    Json doc = Json::Object();
    doc.SetUInt64("big", 0xFEDCBA9876543210ull);
    doc.SetInt64("negative", -5000000000ll);
    doc.SetDouble("precise", 0.1);
    doc.Set("uv", glm::vec2(0.25f, 0.75f));
    doc.SetQuat("spin", glm::quat(0.5f, 0.1f, 0.2f, 0.3f));
    doc.Set("text", "hello");

    // A round trip through text keeps every value
    const Json parsed = Json::Parse(doc.Dump());
    REQUIRE(parsed.GetUInt64("big") == 0xFEDCBA9876543210ull);
    REQUIRE(parsed.GetInt64("negative") == -5000000000ll);
    REQUIRE(parsed.GetDouble("precise") == 0.1);
    REQUIRE(parsed.GetVec2("uv") == glm::vec2(0.25f, 0.75f));
    const glm::quat spin = parsed.GetQuat("spin");
    REQUIRE(spin.w == Catch::Approx(0.5f));
    REQUIRE(spin.x == Catch::Approx(0.1f));
    REQUIRE(spin.z == Catch::Approx(0.3f));
    REQUIRE(parsed.IsString("text"));
    REQUIRE_FALSE(parsed.IsString("big"));

    // A value of the wrong sign or type gives the default
    REQUIRE(parsed.GetUInt64("negative", 7) == 7);
    REQUIRE(parsed.GetInt64("big", 7) == 7);
    REQUIRE(parsed.GetInt64("text", 7) == 7);

    doc.Remove("text");
    doc.Remove("not there");
    REQUIRE_FALSE(doc.Contains("text"));
}
