#include <catch2/catch_all.hpp>

#include "Json.h"
#include "JsonArchive.h"
#include "Components/Transform.h"

using Fling::Transform;

TEST_CASE("A new transform starts dirty", "[transform]")
{
    Transform t;
    REQUIRE(t.IsDirty());
}

TEST_CASE("CalculateWorldMatrix clears the dirty flag", "[transform]")
{
    Transform t;
    t.SetPos(glm::vec3(1.0f, 2.0f, 3.0f));
    Transform::CalculateWorldMatrix(t);

    REQUIRE_FALSE(t.IsDirty());
    REQUIRE(t.GetWorldMat() == t.GetWorldMatrix());
}

TEST_CASE("Setters mark the transform dirty", "[transform]")
{
    Transform t;

    SECTION("SetPos")
    {
        Transform::CalculateWorldMatrix(t);
        t.SetPos(glm::vec3(1.0f));
        REQUIRE(t.IsDirty());
    }

    SECTION("SetRotation")
    {
        Transform::CalculateWorldMatrix(t);
        t.SetRotation(glm::vec3(0.0f, 90.0f, 0.0f));
        REQUIRE(t.IsDirty());
    }

    SECTION("SetScale")
    {
        Transform::CalculateWorldMatrix(t);
        t.SetScale(glm::vec3(2.0f));
        REQUIRE(t.IsDirty());
    }

    SECTION("MarkDirty after a direct field write")
    {
        Transform::CalculateWorldMatrix(t);
        t.m_Pos = glm::vec3(5.0f);
        REQUIRE_FALSE(t.IsDirty());
        t.MarkDirty();
        REQUIRE(t.IsDirty());
    }
}

TEST_CASE("A loaded transform stays dirty so the renderer uploads it", "[transform]")
{
    Transform source;
    source.SetPos(glm::vec3(4.0f, 5.0f, 6.0f));

    Fling::Json json = Fling::Json::Object();
    {
        Fling::JsonArchive saver(json, Fling::JsonArchive::Mode::Saving);
        source.Serialize(saver);
    }

    Transform loaded;
    Transform::CalculateWorldMatrix(loaded);
    REQUIRE_FALSE(loaded.IsDirty());

    Fling::JsonArchive loader(json, Fling::JsonArchive::Mode::Loading);
    loaded.Serialize(loader);

    REQUIRE(loaded.IsDirty());
    REQUIRE(loaded.GetPos() == source.GetPos());
    REQUIRE(loaded.GetWorldMat() == source.GetWorldMatrix());
}

TEST_CASE("The dirty flag does not change transform equality", "[transform]")
{
    Transform a;
    Transform b;
    Transform::CalculateWorldMatrix(a);

    REQUIRE(a.IsDirty() != b.IsDirty());
    REQUIRE(a == b);
}
