#include "FlingTypes.h"
#include "Threading/Lane.h"
#include "Threading/TripleBuffer.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <latch>
#include <thread>
#include <vector>

using namespace Fling;

namespace
{
    /** A value that a torn read shows: the checksum covers the sequence and every word, also on the heap. */
    struct Payload
    {
        uint64 Sequence = 0;
        uint64 Inline[32] = {};
        std::vector<uint64> Heap;
        uint64 Checksum = 0;
    };

    uint64 Mix(uint64 x)
    {
        x ^= x >> 33;
        x *= 0xff51afd7ed558ccdull;
        x ^= x >> 33;
        return x;
    }

    uint64 ChecksumOf(const Payload& payload)
    {
        uint64 sum = Mix(payload.Sequence) ^ payload.Heap.size();
        for (const uint64 word : payload.Inline)
        {
            sum = Mix(sum + word);
        }
        for (const uint64 word : payload.Heap)
        {
            sum = Mix(sum + word);
        }
        return sum;
    }

    void FillPayload(Payload& payload, const uint64 sequence)
    {
        payload.Sequence = sequence;
        for (size_t i = 0; i < 32; ++i)
        {
            payload.Inline[i] = Mix(sequence * 31 + i);
        }
        // A size that changes, so the heap part is reallocated and rewritten too
        payload.Heap.resize(16 + (sequence % 48));
        for (size_t i = 0; i < payload.Heap.size(); ++i)
        {
            payload.Heap[i] = Mix(sequence ^ (i << 20));
        }
        payload.Checksum = ChecksumOf(payload);
    }
} // namespace

TEST_CASE("TripleBuffer: the reader gets the newest value, and nothing before a publish", "[publish][Threading]")
{
    TripleBuffer<int> buffer(-1);
    REQUIRE_FALSE(buffer.AcquireLatest());
    REQUIRE(buffer.ReadSlot() == -1);

    buffer.WriteSlot() = 1;
    buffer.Publish();
    buffer.WriteSlot() = 2;
    buffer.Publish();
    buffer.WriteSlot() = 3;
    buffer.Publish();

    REQUIRE(buffer.AcquireLatest());
    REQUIRE(buffer.ReadSlot() == 3);
    // No new publish: the read slot stays
    REQUIRE_FALSE(buffer.AcquireLatest());
    REQUIRE(buffer.ReadSlot() == 3);

    buffer.WriteSlot() = 4;
    buffer.Publish();
    REQUIRE(buffer.AcquireLatest());
    REQUIRE(buffer.ReadSlot() == 4);
    REQUIRE(buffer.PublishCount() == 4);
}

TEST_CASE("TripleBuffer: the writer never gets the slot the reader holds", "[publish][Threading]")
{
    TripleBuffer<int> buffer(0);
    buffer.WriteSlot() = 10;
    buffer.Publish();
    REQUIRE(buffer.AcquireLatest());
    const int* held = &buffer.ReadSlot();
    for (int i = 0; i < 10; ++i)
    {
        REQUIRE(&buffer.WriteSlot() != held);
        buffer.WriteSlot() = 100 + i;
        buffer.Publish();
    }
    REQUIRE(*held == 10);
}

TEST_CASE("TripleBuffer: two threads for one second, the reader sees only whole values that never go back",
          "[publish][Threading]")
{
    TripleBuffer<Payload> buffer;
    std::atomic<bool> stop{false};
    std::atomic<uint64> torn{0};
    std::atomic<uint64> backwards{0};
    std::atomic<uint64> acquired{0};
    std::atomic<uint64> published{0};
    std::latch start(2);

    std::thread writer(
        [&]()
        {
            start.arrive_and_wait();
            uint64 sequence = 0;
            while (!stop.load(std::memory_order_relaxed))
            {
                FillPayload(buffer.WriteSlot(), ++sequence);
                buffer.Publish();
            }
            published.store(sequence);
        });

    std::thread reader(
        [&]()
        {
            start.arrive_and_wait();
            uint64 last = 0;
            const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            while (std::chrono::steady_clock::now() < end)
            {
                if (!buffer.AcquireLatest())
                {
                    continue;
                }
                const Payload& payload = buffer.ReadSlot();
                if (payload.Checksum != ChecksumOf(payload))
                {
                    torn.fetch_add(1);
                }
                if (payload.Sequence <= last)
                {
                    backwards.fetch_add(1);
                }
                last = payload.Sequence;
                acquired.fetch_add(1);
            }
            stop.store(true);
        });

    writer.join();
    reader.join();

    INFO("published " << published.load() << ", acquired " << acquired.load());
    REQUIRE(torn.load() == 0);
    REQUIRE(backwards.load() == 0);
    REQUIRE(acquired.load() > 100);
    REQUIRE(published.load() >= acquired.load());
}

namespace
{
    std::atomic<int> g_Violations{0};

    void CountViolation(Lane, Lane)
    {
        g_Violations.fetch_add(1);
    }
} // namespace

TEST_CASE("LaneCheck: a scope tags the thread, and only the other lane is a violation", "[publish][Threading]")
{
    REQUIRE(LaneCheck::Current() == Lane::None);
    {
        LaneCheck::Scope game(Lane::Game);
        REQUIRE(LaneCheck::Current() == Lane::Game);
        REQUIRE(LaneCheck::Allows(Lane::Game));
        REQUIRE(LaneCheck::Allows(Lane::None));
        REQUIRE_FALSE(LaneCheck::Allows(Lane::Frame));
        {
            LaneCheck::Scope frame(Lane::Frame);
            REQUIRE(LaneCheck::Current() == Lane::Frame);
        }
        REQUIRE(LaneCheck::Current() == Lane::Game);
    }
    REQUIRE(LaneCheck::Current() == Lane::None);
    // A thread with no lane passes
    REQUIRE(LaneCheck::Allows(Lane::Frame));

    // The tag is per thread
    std::atomic<int> otherLane{-1};
    {
        LaneCheck::Scope game(Lane::Game);
        std::thread other([&]() { otherLane.store(static_cast<int>(LaneCheck::Current())); });
        other.join();
    }
    REQUIRE(otherLane.load() == static_cast<int>(Lane::None));

    g_Violations.store(0);
    const LaneCheck::ViolationHandler previous = LaneCheck::SetViolationHandler(&CountViolation);
    {
        LaneCheck::Scope frame(Lane::Frame);
        LaneCheck::Check(Lane::Game);
        LaneCheck::Check(Lane::Frame);
        FLING_LANE_CHECK(Lane::Game);
    }
    LaneCheck::SetViolationHandler(previous);
    REQUIRE(g_Violations.load() == (FLING_LANE_CHECKS ? 2 : 1));
}
