#include <catch2/catch_all.hpp>

#include "Input/InputDeviceStates.h"
#include "Input/InputMessageProcessor.h"
#include "Input/InputMessageQueue.h"
#include <limits>
#include <memory>
#include <vector>

namespace
{
    using namespace Fling;

    const InputDeviceId DeviceA{1};

    /** Make a message. TimeNs is Sequence * 100. */
    InputMessage Msg(uint64 t_Seq, InputMessageType t_Type, InputKey t_Key)
    {
        InputMessage Message;
        Message.Sequence = t_Seq;
        Message.TimeNs = static_cast<int64>(t_Seq) * 100;
        Message.Device = DeviceA;
        Message.Type = t_Type;
        Message.Key = t_Key;
        return Message;
    }

    /** Push messages 1 to t_Count. Odd sequences press W, even sequences release it. */
    void PushToggles(InputMessageQueue& t_Queue, uint64 t_From, uint64 t_To)
    {
        for (uint64 i = t_From; i <= t_To; ++i)
        {
            t_Queue.Push(Msg(i, (i % 2) ? InputMessageType::KeyDown : InputMessageType::KeyUp, Keys::W));
        }
    }
} // namespace

TEST_CASE("Input message queue", "[Input]")
{
    SECTION("An empty queue has sequence 0")
    {
        InputMessageQueue Queue(8);
        REQUIRE(Queue.NewestSequence() == 0);
        REQUIRE(Queue.OldestSequence() == 0);
        REQUIRE(Queue.Capacity() == 8);

        InputMessage Out;
        REQUIRE_FALSE(Queue.TryGet(1, Out));
        REQUIRE(Queue.CopyRange(0, &Out, 1) == 0);
    }

    SECTION("Capacity 0 is clamped to 1")
    {
        InputMessageQueue Queue(0);
        REQUIRE(Queue.Capacity() >= 1);
        PushToggles(Queue, 1, 3);
        REQUIRE(Queue.NewestSequence() == 3);
        InputMessage Out;
        REQUIRE(Queue.TryGet(3, Out));
        REQUIRE_FALSE(Queue.TryGet(2, Out));
    }

    SECTION("TryGet")
    {
        InputMessageQueue Queue(8);
        PushToggles(Queue, 1, 5);

        InputMessage Out;
        REQUIRE(Queue.TryGet(3, Out));
        REQUIRE(Out.Sequence == 3);
        REQUIRE(Out.Type == InputMessageType::KeyDown);
        REQUIRE_FALSE(Queue.TryGet(0, Out));
        REQUIRE_FALSE(Queue.TryGet(6, Out));
    }

    SECTION("CopyRange respects after, max, and until")
    {
        InputMessageQueue Queue(8);
        PushToggles(Queue, 1, 6);
        InputMessage Out[8];

        REQUIRE(Queue.CopyRange(0, Out, 8) == 6);
        REQUIRE(Out[0].Sequence == 1);
        REQUIRE(Out[5].Sequence == 6);

        REQUIRE(Queue.CopyRange(2, Out, 8) == 4);
        REQUIRE(Out[0].Sequence == 3);

        REQUIRE(Queue.CopyRange(2, Out, 2) == 2);
        REQUIRE(Out[1].Sequence == 4);

        // TimeNs is Sequence * 100, so until 400 stops before sequence 4.
        REQUIRE(Queue.CopyRange(0, Out, 8, 400) == 3);
        REQUIRE(Out[2].Sequence == 3);

        REQUIRE(Queue.CopyRange(6, Out, 8) == 0);
    }

    SECTION("Wrap around keeps the newest messages")
    {
        InputMessageQueue Queue(8);
        PushToggles(Queue, 1, 20);

        REQUIRE(Queue.NewestSequence() == 20);
        REQUIRE(Queue.OldestSequence() == 13);

        InputMessage Out[8];
        REQUIRE(Queue.CopyRange(0, Out, 8) == 0);
        REQUIRE(Queue.CopyRange(11, Out, 8) == 0);
        REQUIRE(Queue.CopyRange(12, Out, 8) == 8);
        REQUIRE(Out[0].Sequence == 13);
        REQUIRE(Out[7].Sequence == 20);

        REQUIRE_FALSE(Queue.TryGet(12, Out[0]));
        REQUIRE(Queue.TryGet(13, Out[0]));
        REQUIRE(Out[0].Sequence == 13);
    }
}

TEST_CASE("Input message processor", "[Input]")
{
    SECTION("Processors consume at their own rate and see every message once")
    {
        auto Queue = std::make_shared<InputMessageQueue>(64);
        InputMessageProcessor Fast(Queue, 0);
        InputMessageProcessor Slow(Queue, 0);

        PushToggles(*Queue, 1, 10);
        Fast.ConsumeUntil();
        REQUIRE(Fast.GetCursor() == 10);
        REQUIRE(Slow.GetCursor() == 0);

        PushToggles(*Queue, 11, 20);
        Fast.ConsumeUntil();
        Slow.Consume(5);
        REQUIRE(Slow.GetCursor() == 5);
        Slow.ConsumeUntil();

        for (const InputMessageProcessor* Processor : {&Fast, &Slow})
        {
            REQUIRE(Processor->GetCursor() == 20);
            const InputKeyState Key = Processor->GetState().GetKeyState(DeviceA, Keys::W);
            REQUIRE(Key.PressCount == 10);
            REQUIRE(Key.ReleaseCount == 10);
            REQUIRE_FALSE(Key.IsHeld);
            REQUIRE_FALSE(Processor->Overran());
        }
    }

    SECTION("Read does not move the cursor")
    {
        auto Queue = std::make_shared<InputMessageQueue>(16);
        InputMessageProcessor Processor(Queue, 0);
        PushToggles(*Queue, 1, 5);

        InputMessage Out[8];
        REQUIRE(Processor.Read(Out, 8) == 5);
        REQUIRE(Processor.Read(Out, 8) == 5);
        REQUIRE(Processor.GetCursor() == 0);
        REQUIRE_FALSE(Processor.GetState().GetKeyState(DeviceA, Keys::W).IsHeld);
        REQUIRE(Processor.GetState().Find(DeviceA) == nullptr);
    }

    SECTION("ConsumeUntil stops at the time")
    {
        auto Queue = std::make_shared<InputMessageQueue>(16);
        InputMessageProcessor Processor(Queue, 0);
        PushToggles(*Queue, 1, 5);

        // Messages 1 to 3 have a time below 400.
        REQUIRE(Processor.ConsumeUntil(400) == 3);
        REQUIRE(Processor.GetCursor() == 3);
        REQUIRE(Processor.GetState().GetKeyState(DeviceA, Keys::W).IsHeld);

        REQUIRE(Processor.ConsumeUntil() == 2);
        REQUIRE(Processor.GetCursor() == 5);
    }

    SECTION("ConsumeUntil reads more than one chunk")
    {
        auto Queue = std::make_shared<InputMessageQueue>(512);
        InputMessageProcessor Processor(Queue, 0);
        PushToggles(*Queue, 1, 200);
        REQUIRE(Processor.ConsumeUntil() == 200);
        REQUIRE(Processor.GetCursor() == 200);
        REQUIRE(Processor.GetState().GetKeyState(DeviceA, Keys::W).PressCount == 100);
    }

    SECTION("A processor can start after a sequence")
    {
        auto Queue = std::make_shared<InputMessageQueue>(16);
        PushToggles(*Queue, 1, 4);
        InputMessageProcessor Processor(Queue, 4);
        PushToggles(*Queue, 5, 6);
        REQUIRE(Processor.ConsumeUntil() == 2);
        REQUIRE(Processor.GetState().GetKeyState(DeviceA, Keys::W).PressCount == 1);
    }

    SECTION("Edges are per processor")
    {
        auto Queue = std::make_shared<InputMessageQueue>(16);
        InputMessageProcessor First(Queue, 0);
        InputMessageProcessor Second(Queue, 0);
        PushToggles(*Queue, 1, 3);
        First.ConsumeUntil();
        Second.ConsumeUntil();

        First.ClearEdges();
        REQUIRE(First.GetState().GetKeyState(DeviceA, Keys::W).PressCount == 0);
        REQUIRE(First.GetState().GetKeyState(DeviceA, Keys::W).IsHeld);
        REQUIRE(Second.GetState().GetKeyState(DeviceA, Keys::W).PressCount == 2);
    }

    SECTION("Overrun, Consume stops at the gap, and Resync recovers")
    {
        auto Queue = std::make_shared<InputMessageQueue>(8);
        InputMessageProcessor Processor(Queue, 0);
        PushToggles(*Queue, 1, 20);

        REQUIRE(Processor.Overran());

        // Message 20 is a release, so build a latest state where W is up, then hold another key.
        Processor.Consume(20);
        REQUIRE(Processor.Overran());
        REQUIRE(Processor.GetCursor() == 0);
        REQUIRE(Processor.GetState().Find(DeviceA) == nullptr);

        PushToggles(*Queue, 21, 21);
        InputDeviceStates Latest;
        for (uint64 i = 1; i <= 21; ++i)
        {
            // Queue only has the newest 8, so rebuild the same messages.
            Latest.Apply(Msg(i, (i % 2) ? InputMessageType::KeyDown : InputMessageType::KeyUp, Keys::W));
        }
        REQUIRE(Latest.GetKeyState(DeviceA, Keys::W).IsHeld);

        Processor.Resync(Latest, 21);
        REQUIRE_FALSE(Processor.Overran());
        REQUIRE(Processor.GetCursor() == 21);
        REQUIRE(Processor.GetState().GetKeyState(DeviceA, Keys::W).IsHeld);

        PushToggles(*Queue, 22, 22);
        REQUIRE(Processor.ConsumeUntil() == 1);
        REQUIRE_FALSE(Processor.GetState().GetKeyState(DeviceA, Keys::W).IsHeld);
    }
}

TEST_CASE("Input message processor with a destroyed queue", "[Input]")
{
    auto Queue = std::make_shared<InputMessageQueue>(16);
    InputMessageProcessor Processor(Queue, 0);
    PushToggles(*Queue, 1, 5);
    REQUIRE(Processor.IsValid());

    Queue.reset();

    REQUIRE_FALSE(Processor.IsValid());
    InputMessage Out[8];
    REQUIRE(Processor.Read(Out, 8) == 0);
    REQUIRE(Processor.ConsumeUntil() == 0);
    REQUIRE_FALSE(Processor.Overran());
    REQUIRE(Processor.GetCursor() == 0);
}
