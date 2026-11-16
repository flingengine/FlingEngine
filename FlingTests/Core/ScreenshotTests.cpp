#include "FlingPaths.h"
#include "Screenshot.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <thread>
#include <vector>

using Fling::Screenshot;
using Fling::ScreenshotResult;
using Fling::String;

TEST_CASE("Screenshot requests wait for the renderer, in order", "[Screenshot]")
{
    // Nothing left from another test
    Screenshot::FailPending("test start");

    const String First = Screenshot::Request("Saved/a.png");
    const String Second = Screenshot::Request("Saved/b.png");
    REQUIRE(First == Fling::FlingPaths::ResolveProjectPath("Saved/a.png"));

    std::vector<Screenshot::Pending> Pending = Screenshot::TakePending();
    REQUIRE(Pending.size() == 2);
    REQUIRE(Pending[0].Path == First);
    REQUIRE(Pending[1].Path == Second);
    // Taken once
    REQUIRE(Screenshot::TakePending().empty());
}

TEST_CASE("Screenshot::HasPending is the free check the renderer makes every frame", "[Screenshot]")
{
    Screenshot::FailPending("test start");
    REQUIRE_FALSE(Screenshot::HasPending());
    // With nothing waiting, TakePending returns at once (no lock, nothing allocated)
    REQUIRE(Screenshot::TakePending().empty());

    Screenshot::Request("Saved/pending.png");
    REQUIRE(Screenshot::HasPending());
    REQUIRE(Screenshot::TakePending().size() == 1);
    REQUIRE_FALSE(Screenshot::HasPending());

    Screenshot::Request("Saved/failed.png");
    Screenshot::FailPending("no renderer");
    REQUIRE_FALSE(Screenshot::HasPending());
}

TEST_CASE("Screenshot with no path gets a new file under Saved/Screenshots", "[Screenshot]")
{
    Screenshot::FailPending("test start");

    const String A = Screenshot::Request();
    const String B = Screenshot::Request();
    REQUIRE(A != B);
    REQUIRE_THAT(A, Catch::Matchers::StartsWith(Fling::FlingPaths::ProjectSavedDir() + "/Screenshots/Screenshot_"));
    REQUIRE_THAT(A, Catch::Matchers::EndsWith(".png"));
    Screenshot::FailPending("test end");
}

TEST_CASE("Screenshot callbacks hear the result", "[Screenshot]")
{
    Screenshot::FailPending("test start");

    std::vector<ScreenshotResult> Results;
    const auto Keep = [&Results](const ScreenshotResult& t_Result)
    {
        Results.push_back(t_Result);
    };

    SECTION("Finish passes the renderer's result on")
    {
        Screenshot::Request("Saved/ok.png", Keep);
        std::vector<Screenshot::Pending> Pending = Screenshot::TakePending();
        REQUIRE(Pending.size() == 1);
        ScreenshotResult Done;
        Done.Path = Pending[0].Path;
        Done.Ok = true;
        Done.Width = 320;
        Done.Height = 180;
        Screenshot::Finish(Pending[0], Done);

        REQUIRE(Results.size() == 1);
        REQUIRE(Results[0].Ok);
        REQUIRE(Results[0].Width == 320);
    }

    SECTION("FailPending fails each waiting request with the reason (no renderer)")
    {
        Screenshot::Request("Saved/one.png", Keep);
        Screenshot::Request("Saved/two.png", Keep);
        Screenshot::FailPending("there is no renderer");

        REQUIRE(Results.size() == 2);
        for (const ScreenshotResult& Result : Results)
        {
            REQUIRE_FALSE(Result.Ok);
            REQUIRE(Result.Error == "there is no renderer");
        }
        REQUIRE_THAT(Results[1].Path, Catch::Matchers::EndsWith("two.png"));
        REQUIRE(Screenshot::TakePending().empty());
    }
}

TEST_CASE("Screenshot requests from many threads all arrive", "[Screenshot]")
{
    Screenshot::FailPending("test start");

    constexpr int ThreadCount = 4;
    constexpr int PerThread = 50;
    std::vector<std::thread> Threads;
    for (int t = 0; t < ThreadCount; ++t)
    {
        Threads.emplace_back(
            [t]()
            {
                for (int i = 0; i < PerThread; ++i)
                {
                    Screenshot::Request("Saved/t" + std::to_string(t) + "_" + std::to_string(i) + ".png");
                }
            });
    }
    for (std::thread& Each : Threads)
    {
        Each.join();
    }
    REQUIRE(Screenshot::TakePending().size() == ThreadCount * PerThread);
    REQUIRE_FALSE(Screenshot::HasPending());
}
