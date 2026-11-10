#include "TypeName.h"

#include <catch2/catch_all.hpp>

namespace TypeNameTest
{
    struct Plain
    {
    };

    class Kept
    {
    };

    enum class Color
    {
        Red,
    };

    struct Outer
    {
        struct Inner
        {
        };
    };

    template <typename T>
    struct Box
    {
    };
} // namespace TypeNameTest

namespace
{
    struct Hidden
    {
    };
} // namespace

using namespace Fling;

TEST_CASE("TypeNameOf gives the qualified name at compile time", "[typename]")
{
    STATIC_REQUIRE(TypeNameOf<int>() == "int");
    STATIC_REQUIRE(TypeNameOf<TypeNameTest::Plain>() == "TypeNameTest::Plain");
    STATIC_REQUIRE(TypeNameOf<TypeNameTest::Kept>() == "TypeNameTest::Kept");
    STATIC_REQUIRE(TypeNameOf<TypeNameTest::Color>() == "TypeNameTest::Color");
    STATIC_REQUIRE(TypeNameOf<TypeNameTest::Outer::Inner>() == "TypeNameTest::Outer::Inner");
    STATIC_REQUIRE(TypeNameOf<TypeNameTest::Box<int>>() == "TypeNameTest::Box<int>");

    // How a compiler spells an anonymous namespace is its own, but the name ends the same way
    STATIC_REQUIRE(TypeNameOf<Hidden>().ends_with("::Hidden"));
    STATIC_REQUIRE(TypeNameOf<TypeNameTest::Plain>() != TypeNameOf<TypeNameTest::Kept>());
}

TEST_CASE("ShortTypeNameOf has no scope and no template arguments", "[typename]")
{
    STATIC_REQUIRE(ShortTypeNameOf<int>() == "int");
    STATIC_REQUIRE(ShortTypeNameOf<TypeNameTest::Plain>() == "Plain");
    STATIC_REQUIRE(ShortTypeNameOf<TypeNameTest::Outer::Inner>() == "Inner");
    STATIC_REQUIRE(ShortTypeNameOf<Hidden>() == "Hidden");
    STATIC_REQUIRE(ShortTypeNameOf<TypeNameTest::Box<TypeNameTest::Plain>>() == "Box");
}
