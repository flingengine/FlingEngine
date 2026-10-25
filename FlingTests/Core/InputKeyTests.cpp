#include <catch2/catch_all.hpp>

#include "Input/InputKey.h"
#include <string>
#include <unordered_map>

TEST_CASE("Input key names", "[Input]")
{
    using namespace Fling;

    SECTION("Every key round trips through its name")
    {
        for (uint16 i = 1; i < static_cast<uint16>(InputKeyId::Count); ++i)
        {
            const InputKey Key{static_cast<InputKeyId>(i)};
            REQUIRE(Key.IsValid());
            REQUIRE(InputKey::FromName(Key.GetName()) == Key);
        }
    }

    SECTION("Unknown and Invalid names give an invalid key")
    {
        REQUIRE(InputKey::FromName("NotAKey") == Keys::Invalid);
        REQUIRE(InputKey::FromName("Invalid") == Keys::Invalid);
        REQUIRE(InputKey::FromName("") == Keys::Invalid);
        REQUIRE(InputKey::FromName("leftshift") == Keys::Invalid);
        REQUIRE_FALSE(InputKey::FromName("NotAKey").IsValid());
    }

    SECTION("Invalid key has the Invalid details")
    {
        REQUIRE_FALSE(Keys::Invalid.IsValid());
        REQUIRE(std::string(Keys::Invalid.GetName()) == "Invalid");
        REQUIRE(std::string(InputKey(InputKeyId::Count).GetName()) == "Invalid");
    }

    SECTION("Names")
    {
        REQUIRE(std::string(Keys::LeftShift.GetName()) == "LeftShift");
        REQUIRE(InputKey::FromName("W") == Keys::W);
    }
}

TEST_CASE("Input key flags", "[Input]")
{
    using namespace Fling;

    REQUIRE(Keys::GamepadLeftX.IsAxis());
    REQUIRE(Keys::GamepadLeftX.IsGamepadKey());
    REQUIRE_FALSE(Keys::GamepadLeftX.IsKeyboardKey());
    REQUIRE(Keys::LeftShift.IsModifier());
    REQUIRE(Keys::LeftShift.IsKeyboardKey());
    REQUIRE(Keys::MouseLeft.IsMouseButton());
    REQUIRE_FALSE(Keys::MouseLeft.IsKeyboardKey());
    REQUIRE(Keys::W.IsKeyboardKey());
    REQUIRE_FALSE(Keys::W.IsModifier());
    REQUIRE_FALSE(Keys::W.IsAxis());
}

TEST_CASE("Input key hash and equality", "[Input]")
{
    using namespace Fling;

    std::unordered_map<InputKey, int> Map;
    Map[Keys::W] = 1;
    Map[Keys::A] = 2;
    Map[Keys::W] = 3;

    REQUIRE(Map.size() == 2);
    REQUIRE(Map[Keys::W] == 3);
    REQUIRE(Map[Keys::A] == 2);
    REQUIRE(Keys::W == InputKey(InputKeyId::W));
    REQUIRE(Keys::W != Keys::A);
}
