#include "Profiler.h"

#include <catch2/catch_test_macros.hpp>
#include <thread>

using namespace Fling;

TEST_CASE("Profiler counts scopes only while it is on", "[Profiler]")
{
#ifndef FLING_SHIPPING
    Profiler::SetEnabled(false);
    Profiler::Reset();
    {
        FLING_PROFILE_SCOPE("ProfilerTest.Off");
    }
    REQUIRE(Profiler::Report(0.0).find("ProfilerTest.Off") == String::npos);

    Profiler::SetEnabled(true);
    for (int i = 0; i < 3; ++i)
    {
        FLING_PROFILE_SCOPE("ProfilerTest.On");
    }
    std::thread Other([]() { FLING_PROFILE_SCOPE("ProfilerTest.On"); });
    Other.join();
    Profiler::SetEnabled(false);

    // Scopes with one name add up, across threads: 3 calls here and 1 on the other thread
    const String Report = Profiler::Report(1.0);
    const size_t Row = Report.find("ProfilerTest.On");
    REQUIRE(Row != String::npos);
    REQUIRE(Report.find(" 4 ", Row) != String::npos);
    Profiler::Reset();
#endif
}
