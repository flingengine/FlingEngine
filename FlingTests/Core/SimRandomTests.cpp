// SimRandom, the random stream that simulation code may use: the same seed and stream give
// the same numbers, the counter is the whole state, and stream names hash by FNV-1a.

#include "SimRandom.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

TEST_CASE("SimRandom gives the same numbers for the same seed and stream", "[Random]")
{
    SimRandom A(1, SimRandom::StreamOf("Test"));
    SimRandom B(1, SimRandom::StreamOf("Test"));
    std::vector<uint64> First;
    for (int i = 0; i < 100; ++i)
    {
        First.push_back(A.NextU64());
        REQUIRE(First.back() == B.NextU64());
    }

    // Fixed values: a change to the generator changes every simulation that uses it
    SimRandom Golden(1, 0);
    REQUIRE(Golden.NextU64() == 0x4181B152FB77616Full);
    REQUIRE(Golden.NextU64() == 0x169C646D52269D62ull);
    REQUIRE(Golden.NextU64() == 0x4A5DE8D8D53B7280ull);

    // Another seed or stream is another sequence
    REQUIRE(SimRandom(2, SimRandom::StreamOf("Test")).NextU64() != First[0]);
    REQUIRE(SimRandom(1, SimRandom::StreamOf("Other")).NextU64() != First[0]);

    // The counter is the whole state: set it back to repeat draws
    SimRandom C(1, SimRandom::StreamOf("Test"));
    C.SetCounter(50);
    REQUIRE(C.NextU64() == First[50]);
    REQUIRE(C.GetCounter() == 51);

    // Ranges
    SimRandom D(7, 3);
    bool SawLow = false;
    bool SawHigh = false;
    for (int i = 0; i < 10'000; ++i)
    {
        const float F = D.Range(2.0f, -1.0f);
        REQUIRE(F >= -1.0f);
        REQUIRE(F < 2.0f);
        const int32 N = D.RangeInclusive(-3, 3);
        REQUIRE(N >= -3);
        REQUIRE(N <= 3);
        SawLow = SawLow || N == -3;
        SawHigh = SawHigh || N == 3;
        const glm::vec3 V = D.Range(glm::vec3(0.0f), glm::vec3(1.0f, 2.0f, 3.0f));
        REQUIRE(V.z < 3.0f);
    }
    REQUIRE(SawLow);
    REQUIRE(SawHigh);
}

TEST_CASE("SimRandom stream names are FNV-1a", "[Random]")
{
    static_assert(SimRandom::StreamOf("") == 14695981039346656037ull);
    static_assert(SimRandom::StreamOf("a") == 0xAF63DC4C8601EC8Cull);
    REQUIRE(SimRandom::StreamOf("Sandbox.LightingTest") != SimRandom::StreamOf("Sandbox.LightingTesT"));
}
