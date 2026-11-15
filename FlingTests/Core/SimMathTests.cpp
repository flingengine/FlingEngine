// SimMath, the math that simulation code may use: the exact bits of every function for a table of inputs, the same
// on every platform and compiler, and within one ulp of the true value.

#include "FlingMath.h"
#include "FlingTypes.h"
#include "SimMath.h"

#include <bit>
#include <catch2/catch_all.hpp>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <vector>

using namespace Fling;

namespace
{
    uint32 Bits(const float t_Value)
    {
        return std::bit_cast<uint32>(t_Value);
    }

    /** True when two floats are equal, or finite, of one sign and at most one float apart. */
    bool WithinOneUlp(const float t_A, const float t_B)
    {
        if (t_A == t_B)
        {
            return true;
        }
        if (std::signbit(t_A) != std::signbit(t_B) || !std::isfinite(t_A) || !std::isfinite(t_B))
        {
            return false;
        }
        return std::abs(std::bit_cast<int32>(t_A) - std::bit_cast<int32>(t_B)) <= 1;
    }

    /** The true value for a float input: the double result rounded once to float. */
    template <typename F>
    float Reference(F t_Function, const float t_X)
    {
        return static_cast<float>(t_Function(static_cast<double>(t_X)));
    }
} // namespace

// The tables below are fixed values. GCC and Clang, -O0 and -O2, give these bits on Linux; MSVC and clang-cl must give
// the same, because SimMath uses only operations that IEEE 754 defines exactly. A failure on one platform means that
// platform simulates differently from the others (the pinned tick dumps in Sandbox/CMakeLists.txt fail too). A change
// to SimMath itself changes these and every state hash.
TEST_CASE("SimMath: sin, cos, tan and atan give the same bits on every platform", "[math]")
{
    struct Golden
    {
        float X;
        uint32 Sin;
        uint32 Cos;
        uint32 Tan;
        uint32 Atan;
    };
    const Golden Cases[] = {
        {0.0f, 0x00000000u, 0x3F800000u, 0x00000000u, 0x00000000u},
        {-0.0f, 0x80000000u, 0x3F800000u, 0x80000000u, 0x80000000u},
        {0x1p-1f, 0x3EF57744u, 0x3F60A940u, 0x3F0BDA7Bu, 0x3EED6338u},
        {0x1p+0f, 0x3F576AA4u, 0x3F0A5140u, 0x3FC75923u, 0x3F490FDBu},
        {-0x1p+1f, 0xBF68C7B7u, 0xBED51133u, 0x400BD7B1u, 0xBF8DB70Du},
        {0x1.921fb6p+1f, 0xB3BBBD2Eu, 0xBF800000u, 0x33BBBD2Eu, 0x3FA19DC5u},
        {0x1.921fb6p-1f, 0x3F3504F3u, 0x3F3504F3u, 0x3F800000u, 0x3F2A7026u},
        {0x1.8p+0f, 0x3F7F5BD5u, 0x3D90DEAAu, 0x41619F6Bu, 0x3F7B985Fu},
        {0x1.9p+6f, 0xBF01A12Eu, 0x3F5CC0EEu, 0xBF1653A7u, 0x3FC7C82Fu},
        {0x1.81cd6cp+13f, 0xBF344B08u, 0x3F35BE20u, 0xBF7DF549u, 0x3FC90D33u},
        {-0x1.99999ap-4f, 0xBDCC7577u, 0x3F7EB898u, 0xBDCD7C44u, 0xBDCC1F14u},
        {0x1.0c6f7ap-20f, 0x358637BDu, 0x3F800000u, 0x358637BDu, 0x358637BDu},
    };
    for (const Golden& Case : Cases)
    {
        CAPTURE(Case.X);
        float Sine = 0.0f;
        float Cosine = 0.0f;
        SimMath::SinCos(Case.X, Sine, Cosine);
        CHECK(Bits(Sine) == Case.Sin);
        CHECK(Bits(Cosine) == Case.Cos);
        CHECK(Bits(SimMath::Sin(Case.X)) == Case.Sin);
        CHECK(Bits(SimMath::Cos(Case.X)) == Case.Cos);
        CHECK(Bits(SimMath::Tan(Case.X)) == Case.Tan);
        CHECK(Bits(SimMath::Atan(Case.X)) == Case.Atan);
    }
}

TEST_CASE("SimMath: asin and acos give the same bits on every platform", "[math]")
{
    struct Golden
    {
        float X;
        uint32 Asin;
        uint32 Acos;
    };
    const Golden Cases[] = {
        {0.0f, 0x00000000u, 0x3FC90FDBu},
        {-0.0f, 0x80000000u, 0x3FC90FDBu},
        {0x1p-1f, 0x3F060A92u, 0x3F860A92u},
        {-0x1p-1f, 0xBF060A92u, 0x40060A92u},
        {0x1p+0f, 0x3FC90FDBu, 0x00000000u},
        {-0x1p+0f, 0xBFC90FDBu, 0x40490FDBu},
        {0x1.99999ap-4f, 0x3DCD2494u, 0x3FBC3D91u},
        {0x1.ff7ceep-1f, 0x3FC35650u, 0x3D37315Au},
        {-0x1.8p-1f, 0xBF591A99u, 0x401ACE93u},
        {0x1.0c6f7ap-20f, 0x358637BDu, 0x3FC90FD2u},
    };
    for (const Golden& Case : Cases)
    {
        CAPTURE(Case.X);
        CHECK(Bits(SimMath::Asin(Case.X)) == Case.Asin);
        CHECK(Bits(SimMath::Acos(Case.X)) == Case.Acos);
    }
}

TEST_CASE("SimMath: atan2 gives the same bits on every platform", "[math]")
{
    struct Golden
    {
        float Y;
        float X;
        uint32 Atan2;
    };
    const Golden Cases[] = {
        {0x1p+0f, 0x1p+0f, 0x3F490FDBu},
        {0x1p+0f, -0x1p+0f, 0x4016CBE4u},
        {-0x1p+0f, -0x1p+0f, 0xC016CBE4u},
        {-0x1p+0f, 0x1p+0f, 0xBF490FDBu},
        {0.0f, -0x1p+0f, 0x40490FDBu},
        {-0.0f, -0x1p+0f, 0xC0490FDBu},
        {0.0f, 0.0f, 0x00000000u},
        {-0.0f, -0.0f, 0xC0490FDBu},
        {0x1.8p+1f, 0x1p-1f, 0x3FB3EC43u},
        {-0x1.4p+1f, 0x1.cp+2f, 0xBEAFA0D5u},
        {0x1.0624dep-10f, 0x1.f4p+9f, 0x358637BDu},
    };
    for (const Golden& Case : Cases)
    {
        CAPTURE(Case.Y, Case.X);
        CHECK(Bits(SimMath::Atan2(Case.Y, Case.X)) == Case.Atan2);
    }
}

TEST_CASE("SimMath: exp, log and pow give the same bits on every platform", "[math]")
{
    struct Unary
    {
        float X;
        uint32 Result;
    };
    const Unary ExpCases[] = {
        {0.0f, 0x3F800000u},
        {0x1p+0f, 0x402DF854u},
        {-0x1p+0f, 0x3EBC5AB2u},
        {0x1p-1f, 0x3FD3094Cu},
        {0x1.4p+3f, 0x46AC14EEu},
        {-0x1.4p+3f, 0x383E6BCEu},
        {0x1.6p+6f, 0x7EF882B7u},
        {-0x1.5cp+6f, 0x00B33687u},
        {0x1.0624dep-10f, 0x3F8020C9u},
        {0x1.26bb1ap+1f, 0x411FFFFEu},
    };
    for (const Unary& Case : ExpCases)
    {
        CAPTURE(Case.X);
        CHECK(Bits(SimMath::Exp(Case.X)) == Case.Result);
    }

    const Unary LogCases[] = {
        {0x1p+0f, 0x00000000u},
        {0x1p+1f, 0x3F317218u},
        {0x1p-1f, 0xBF317218u},
        {0x1.4p+3f, 0x40135D8Eu},
        {0x1.5bf0a8p+1f, 0x3F7FFFFFu},
        {0x1.0c6f7ap-20f, 0xC15D0C55u},
        {0x1.93e594p+99f, 0x428A27B5u},
        {0x1.ff7ceep-1f, 0xBA8322CAu},
        {0x1.00418ap+0f, 0x3A83033Bu},
        {0x1.8p+1f, 0x3F8C9F54u},
    };
    for (const Unary& Case : LogCases)
    {
        CAPTURE(Case.X);
        CHECK(Bits(SimMath::Log(Case.X)) == Case.Result);
    }

    struct Binary
    {
        float Base;
        float Exponent;
        uint32 Result;
    };
    const Binary PowCases[] = {
        {0x1p+1f, 0x1.4p+3f, 0x44800000u},
        {0x1p+1f, 0x1p-1f, 0x3FB504F3u},
        {0x1.4p+3f, -0x1p+1f, 0x3C23D70Au},
        {-0x1p+1f, 0x1.8p+1f, 0xC1000000u},
        {-0x1p+1f, 0x1p+1f, 0x40800000u},
        {0x1p-1f, 0x1p-1f, 0x3F3504F3u},
        {0x1.8p+0f, 0x1.4p+1f, 0x40305CFFu},
        {0x1.2p+3f, 0x1p-1f, 0x40400000u},
        {0.0f, 0x1p+1f, 0x00000000u},
        {0x1.cp+2f, 0.0f, 0x3F800000u},
    };
    for (const Binary& Case : PowCases)
    {
        CAPTURE(Case.Base, Case.Exponent);
        CHECK(Bits(SimMath::Pow(Case.Base, Case.Exponent)) == Case.Result);
    }
}

TEST_CASE("SimMath: the rotation helpers give the same bits on every platform", "[math]")
{
    const auto QuatBits = [](const glm::quat& t_Q)
    {
        return std::vector<uint32>{Bits(t_Q.x), Bits(t_Q.y), Bits(t_Q.z), Bits(t_Q.w)};
    };

    const glm::quat Turn = SimMath::AngleAxis(0.7f, glm::normalize(glm::vec3(1.0f, 2.0f, 3.0f)));
    CHECK(QuatBits(Turn) == std::vector<uint32>{0x3DBBAF7Au, 0x3E3BAF7Au, 0x3E8CC39Bu, 0x3F707ABBu});

    const glm::quat Euler = SimMath::FromYawPitchRoll(0.4f, -0.3f, 1.1f);
    CHECK(QuatBits(Euler) == std::vector<uint32>{0xBCB5BBCEu, 0x3E79E0A7u, 0x3F0825C8u, 0x3F4F857Fu});

    float Yaw = 0.0f;
    float Pitch = 0.0f;
    float Roll = 0.0f;
    SimMath::ToYawPitchRoll(glm::mat3_cast(Euler), Yaw, Pitch, Roll);
    CHECK(Bits(Yaw) == 0x3ECCCCCDu);
    CHECK(Bits(Pitch) == 0xBE99999Au);
    CHECK(Bits(Roll) == 0x3F8CCCCDu);

    const glm::quat Between = SimMath::Slerp(Turn, Euler, 0.3f);
    CHECK(QuatBits(Between) == std::vector<uint32>{0x3D6D5629u, 0x3E509F02u, 0x3EB6529Fu, 0x3F68FE05u});
}

TEST_CASE("SimMath is within one ulp of the true value", "[math]")
{
    // On Linux every function matched the reference exactly at every point of these sweeps; one ulp leaves room for
    // a platform whose double libm is itself off by a fraction of a float ulp.
    for (double d = -50.0; d <= 50.0; d += 0.0173)
    {
        const float x = static_cast<float>(d);
        CAPTURE(x);
        REQUIRE(WithinOneUlp(SimMath::Sin(x), Reference([](double v) { return std::sin(v); }, x)));
        REQUIRE(WithinOneUlp(SimMath::Cos(x), Reference([](double v) { return std::cos(v); }, x)));
        REQUIRE(WithinOneUlp(SimMath::Tan(x), Reference([](double v) { return std::tan(v); }, x)));
        REQUIRE(WithinOneUlp(SimMath::Atan(x), Reference([](double v) { return std::atan(v); }, x)));
        const float Other = 0.37f * x + 1.3f;
        REQUIRE(WithinOneUlp(SimMath::Atan2(x, Other),
                             static_cast<float>(std::atan2(static_cast<double>(x), static_cast<double>(Other)))));
        const float Unit = static_cast<float>(d / 50.0);
        REQUIRE(WithinOneUlp(SimMath::Asin(Unit), Reference([](double v) { return std::asin(v); }, Unit)));
        REQUIRE(WithinOneUlp(SimMath::Acos(Unit), Reference([](double v) { return std::acos(v); }, Unit)));
        const float Power = static_cast<float>(d * 1.7);
        REQUIRE(WithinOneUlp(SimMath::Exp(Power), Reference([](double v) { return std::exp(v); }, Power)));
        const float Positive = std::fabs(x) * 3.1f + 1.0e-3f;
        REQUIRE(WithinOneUlp(SimMath::Log(Positive), Reference([](double v) { return std::log(v); }, Positive)));
        const float Base = std::fabs(x) * 0.3f + 0.01f;
        const float Exponent = static_cast<float>(d * 0.07);
        REQUIRE(WithinOneUlp(SimMath::Pow(Base, Exponent),
                             static_cast<float>(std::pow(static_cast<double>(Base), static_cast<double>(Exponent)))));
    }
    // Sin and Cos far from zero, where the range reduction does the work
    for (double d = -2000.0; d <= 2000.0; d += 0.731)
    {
        const float x = static_cast<float>(d);
        CAPTURE(x);
        REQUIRE(WithinOneUlp(SimMath::Sin(x), Reference([](double v) { return std::sin(v); }, x)));
        REQUIRE(WithinOneUlp(SimMath::Cos(x), Reference([](double v) { return std::cos(v); }, x)));
    }
}

TEST_CASE("SimMath handles zeros, infinities and NaN like std", "[math]")
{
    constexpr float Infinity = std::numeric_limits<float>::infinity();
    constexpr float NaN = std::numeric_limits<float>::quiet_NaN();
    CHECK(Bits(SimMath::Sin(-0.0f)) == 0x80000000u);
    CHECK(SimMath::Cos(-0.0f) == 1.0f);
    CHECK(std::isnan(SimMath::Sin(Infinity)));
    CHECK(std::isnan(SimMath::Cos(-Infinity)));
    CHECK(std::isnan(SimMath::Tan(NaN)));
    CHECK(std::isnan(SimMath::Asin(1.5f)));
    CHECK(std::isnan(SimMath::Acos(-1.5f)));
    CHECK(SimMath::Atan(Infinity) == SimMath::Atan2(1.0f, 0.0f));
    CHECK(Bits(SimMath::Atan2(-0.0f, 0.0f)) == 0x80000000u);
    CHECK(SimMath::Exp(-Infinity) == 0.0f);
    CHECK(SimMath::Exp(Infinity) == Infinity);
    CHECK(SimMath::Exp(100.0f) == Infinity);
    CHECK(SimMath::Log(0.0f) == -Infinity);
    CHECK(std::isnan(SimMath::Log(-1.0f)));
    CHECK(SimMath::Pow(NaN, 0.0f) == 1.0f);
    CHECK(std::isnan(SimMath::Pow(-2.0f, 0.5f)));
    CHECK(SimMath::Pow(0.0f, -1.0f) == Infinity);
    CHECK(SimMath::Pow(-0.0f, -1.0f) == -Infinity);
    CHECK(SimMath::Pow(0.5f, Infinity) == 0.0f);
    CHECK(SimMath::Pow(-1.0f, Infinity) == 1.0f);
}
