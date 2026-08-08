#include <catch2/catch_all.hpp>

#include "Misc/StringUtils.h"
#include <string>
#include <string_view>

TEST_CASE("String Utils", "[String Utils]")
{
	using namespace Fling;

	SECTION("Parse bool")
	{
		REQUIRE(StringUtils::ParseAs<bool>("true", false) == true);
		REQUIRE(StringUtils::ParseAs<bool>("1", false) == true);
		REQUIRE(StringUtils::ParseAs<bool>("false", true) == false);
		REQUIRE(StringUtils::ParseAs<bool>("0", true) == false);

		// Garbage input should fall back to the default
		REQUIRE(StringUtils::ParseAs<bool>("notABool", true) == true);
	}

	SECTION("Parse int")
	{
		REQUIRE(StringUtils::ParseAs<int>("776", -1) == 776);
		REQUIRE(StringUtils::ParseAs<int>("-42", 0) == -42);

		// A leading zero should not be treated as an octal prefix
		REQUIRE(StringUtils::ParseAs<int>("010", -1) == 10);

		// Garbage input should fall back to the default
		REQUIRE(StringUtils::ParseAs<int>("notANumber", -1) == -1);
	}

	SECTION("Parse int32")
	{
		REQUIRE(StringUtils::ParseAs<int32>("2147483647", 0) == 2147483647);
	}

	SECTION("Parse float")
	{
		REQUIRE(StringUtils::ParseAs<float>("3.14", 0.0f) == Catch::Approx(3.14f));

		// Garbage input should fall back to the default
		REQUIRE(StringUtils::ParseAs<float>("notANumber", -1.0f) == Catch::Approx(-1.0f));
	}

	SECTION("Parse double")
	{
		REQUIRE(StringUtils::ParseAs<double>("-1123.56", 0.0) == Catch::Approx(-1123.56));

		// Garbage input should fall back to the default
		REQUIRE(StringUtils::ParseAs<double>("notANumber", -1.0) == Catch::Approx(-1.0));
	}

	SECTION("Parse string")
	{
		REQUIRE(StringUtils::ParseAs<std::string>("I like this format", "default") == "I like this format");
	}

	SECTION("Empty input falls back to default")
	{
		REQUIRE(StringUtils::ParseAs<int>("", 42) == 42);
		REQUIRE(StringUtils::ParseAs<double>("", 3.5) == Catch::Approx(3.5));
		REQUIRE(StringUtils::ParseAs<std::string>("", "default") == "default");
	}
}
