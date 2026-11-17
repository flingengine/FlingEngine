#include "FrameSync.h"

#include <catch2/catch_all.hpp>
#include <vector>

using namespace Fling;

TEST_CASE("Frame sync: the slot goes round, and a frame is done FramesInFlight frames later", "[FrameSync]")
{
    FrameSync::Reset();
    for (uint64 Frame = 0; Frame < 8; ++Frame)
    {
        FrameSync::Begin(Frame);
        REQUIRE(FrameSync::Frame() == Frame);
        REQUIRE(FrameSync::Slot() == Frame % FrameSync::FramesInFlight);
    }

    FrameSync::Begin(5);
    // The frame before the slot's last frame is done, the one in flight is not
    REQUIRE(FrameSync::IsDone(5 - FrameSync::FramesInFlight));
    REQUIRE_FALSE(FrameSync::IsDone(5 - FrameSync::FramesInFlight + 1));
    REQUIRE_FALSE(FrameSync::IsDone(5));
    FrameSync::Reset();
}

TEST_CASE("Frame sync: an idle device finishes every frame before the current one", "[FrameSync]")
{
    FrameSync::Reset();
    FrameSync::Begin(10);
    REQUIRE_FALSE(FrameSync::IsDone(9));
    FrameSync::MarkIdle();
    REQUIRE(FrameSync::IsDone(9));
    // The frame that is recorded now is not submitted yet, so it is not done
    REQUIRE_FALSE(FrameSync::IsDone(10));
    FrameSync::Reset();
}

TEST_CASE("Retire list: an item goes when the frames up to its own are done", "[FrameSync]")
{
    FrameSync::Reset();
    RetireList<int> Retired;
    std::vector<int> Freed;
    auto Free = [&Freed](const int t_Item)
    {
        Freed.push_back(t_Item);
    };

    FrameSync::Begin(3);
    Retired.Add(30);
    FrameSync::Begin(4);
    Retired.Add(40);
    Retired.Add(41);

    // Frame 3 may still be on the GPU while frame 4 is recorded
    Retired.Release(Free);
    REQUIRE(Freed.empty());

    FrameSync::Begin(3 + FrameSync::FramesInFlight);
    Retired.Release(Free);
    REQUIRE(Freed == std::vector<int>{30});

    FrameSync::Begin(4 + FrameSync::FramesInFlight);
    Retired.Release(Free);
    REQUIRE(Freed == std::vector<int>{30, 40, 41});
    REQUIRE(Retired.Empty());
    FrameSync::Reset();
}

TEST_CASE("Retire list: ReleaseAll frees what is left, done or not", "[FrameSync]")
{
    FrameSync::Reset();
    RetireList<int> Retired;
    FrameSync::Begin(7);
    Retired.Add(1);
    Retired.Add(2);

    std::vector<int> Freed;
    Retired.ReleaseAll([&Freed](const int t_Item) { Freed.push_back(t_Item); });
    REQUIRE(Freed == std::vector<int>{1, 2});
    REQUIRE(Retired.Empty());
    FrameSync::Reset();
}
