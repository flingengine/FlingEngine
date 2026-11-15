#include "../Common/TestScopes.h"
#include "Input/Input.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputPlayback.h"
#include "Input/InputRecorder.h"
#include "Input/InputRecording.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Misc/CommandLine.h"
#include "Strings/String.h"
#include "Threading/Lane.h"

#include <atomic>
#include <catch2/catch_all.hpp>
#include <chrono>
#include <filesystem>
#include <thread>
#include <vector>

using namespace Fling;

// Input with a writer thread and reader threads (Docs/threading-model.md section 8): the
// seqlock queue, the latest-state snapshot, the watermark, capture as a message, and
// the tick input provider that waits for the writer to send a replay cut.

namespace
{
    constexpr InputDeviceId StressDevice{7};

    uint64 Mix(uint64 t_Value)
    {
        t_Value = (t_Value ^ (t_Value >> 30)) * 0xBF58476D1CE4E5B9ull;
        t_Value = (t_Value ^ (t_Value >> 27)) * 0x94D049BB133111EBull;
        return t_Value ^ (t_Value >> 31);
    }

    /** A mouse move whose every field is a function of its sequence: a checksum per message. */
    InputMessage StressMessage(const uint64 t_Sequence)
    {
        const uint64 Hash = Mix(t_Sequence);
        InputMessage Message;
        Message.Device = StressDevice;
        Message.Type = InputMessageType::MouseMove;
        Message.TimeNs = static_cast<int64>(t_Sequence) * 1000;
        Message.DeltaX = static_cast<int32>(static_cast<uint32>(Hash));
        Message.DeltaY = static_cast<int32>(static_cast<uint32>(Hash >> 32));
        Message.Scancode = static_cast<uint32>(Hash >> 16);
        Message.CodePoint = static_cast<uint32>(Hash >> 24);
        Message.PositionX = static_cast<float>(Hash & 0xFFFF);
        Message.PositionY = static_cast<float>((Hash >> 16) & 0xFFFF);
        Message.WheelX = static_cast<float>((Hash >> 32) & 0xFFFF);
        Message.WheelY = static_cast<float>((Hash >> 48) & 0xFFFF);
        return Message;
    }

    bool IsIntact(const InputMessage& t_Message)
    {
        const InputMessage Expected = StressMessage(t_Message.Sequence);
        return t_Message.Device == Expected.Device && t_Message.Type == Expected.Type &&
               t_Message.TimeNs == Expected.TimeNs && t_Message.DeltaX == Expected.DeltaX &&
               t_Message.DeltaY == Expected.DeltaY && t_Message.Scancode == Expected.Scancode &&
               t_Message.CodePoint == Expected.CodePoint && t_Message.PositionX == Expected.PositionX &&
               t_Message.PositionY == Expected.PositionY && t_Message.WheelX == Expected.WheelX &&
               t_Message.WheelY == Expected.WheelY;
    }

    bool IsIntact(const InputMouseSample& t_Sample)
    {
        const InputMessage Expected = StressMessage(t_Sample.Sequence);
        return t_Sample.DeltaX == Expected.DeltaX && t_Sample.DeltaY == Expected.DeltaY;
    }

    /** What one reader thread saw. Checked on the test thread after the join (Catch2 is not thread safe). */
    struct ReaderResult
    {
        uint64 Messages = 0;
        uint64 Torn = 0;
        uint64 OutOfOrder = 0;
        uint64 Gaps = 0;
        uint64 Overruns = 0;
        uint64 Reads = 0;
    };

    /** Follows the sequences of one reader: increasing by one, or a gap after an overrun. */
    void Check(ReaderResult& t_Result, uint64& t_Expected, const uint64 t_Sequence, const bool t_Intact)
    {
        if (!t_Intact)
        {
            ++t_Result.Torn;
        }
        if (t_Sequence < t_Expected)
        {
            ++t_Result.OutOfOrder;
        }
        else if (t_Sequence > t_Expected)
        {
            ++t_Result.Gaps;
        }
        t_Expected = t_Sequence + 1;
        ++t_Result.Messages;
    }

    struct StressConfig
    {
        uint32 Capacity = InputMessageQueue::DefaultCapacity;

        /** True: the writer waits for the slowest reader, so nothing may be lost. */
        bool Throttle = true;

        std::chrono::milliseconds Duration{1500};
    };

    struct StressResult
    {
        uint64 Written = 0;
        ReaderResult Tick;
        ReaderResult Raw;
        ReaderResult Pending;
    };

    /**
     * One writer thread (the handler) and three readers on their own threads: a tick
     * builder, a raw processor that checks whole messages, and a pending reader that
     * reads after the tick's published cut.
     */
    StressResult RunStress(const StressConfig& t_Config)
    {
        std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create(t_Config.Capacity);
        std::atomic<bool> WriterDone{false};
        std::atomic<uint64> Written{0};
        std::atomic<uint64> TickCut{0};
        std::atomic<uint64> RawCursor{0};
        std::atomic<uint64> PendingFloor{0};
        StressResult Result;

        // The readers start before the first message, so they see every message
        TickInputBuilder Builder(Handler);
        InputMessageProcessor Processor = Handler->CreateProcessor();

        const uint64 Limit = t_Config.Capacity / 2;
        std::thread Writer(
            [&]()
            {
                LaneCheck::Scope Lane(Lane::Frame);
                const auto End = std::chrono::steady_clock::now() + t_Config.Duration;
                uint64 Sequence = 0;
                while (std::chrono::steady_clock::now() < End)
                {
                    for (int i = 0; i < 64; ++i)
                    {
                        if (t_Config.Throttle)
                        {
                            while (Sequence - std::min({TickCut.load(), RawCursor.load(), PendingFloor.load()}) >=
                                   Limit)
                            {
                                std::this_thread::yield();
                            }
                        }
                        Handler->HandleMessage(StressMessage(++Sequence));
                    }
                    Written.store(Sequence, std::memory_order_release);
                }
                WriterDone.store(true, std::memory_order_release);
            });

        const auto IsFinished = [&](const uint64 t_Cursor)
        {
            return WriterDone.load(std::memory_order_acquire) && t_Cursor >= Written.load(std::memory_order_acquire);
        };

        std::thread TickReader(
            [&]()
            {
                LaneCheck::Scope Lane(Lane::Game);
                TickInput Tick;
                uint64 Expected = 1;
                uint64 Spin = 0;
                do
                {
                    Builder.Build(Tick);
                    for (const InputMouseSample& Sample : Tick.Slice.MouseSamples)
                    {
                        Check(Result.Tick, Expected, Sample.Sequence, IsIntact(Sample));
                    }
                    TickCut.store(Tick.Slice.Cut.LastSequence, std::memory_order_release);
                    ++Result.Tick.Reads;
                    // Fall behind now and then, so an unthrottled writer overruns this reader
                    if (!t_Config.Throttle && ++Spin % 64 == 0)
                    {
                        std::this_thread::sleep_for(std::chrono::microseconds(200));
                    }
                } while (!IsFinished(Tick.Slice.Cut.LastSequence));
            });

        std::thread RawReader(
            [&]()
            {
                InputMessage Chunk[48];
                uint64 Expected = 1;
                do
                {
                    if (Processor.Overran())
                    {
                        ++Result.Raw.Overruns;
                        const InputStateSnapshot Latest = Handler->GetLatestSnapshot();
                        Processor.Resync(Latest.States, Latest.Sequence);
                    }
                    const uint32 Got = Processor.Read(Chunk, 48);
                    for (uint32 i = 0; i < Got; ++i)
                    {
                        Check(Result.Raw, Expected, Chunk[i].Sequence, IsIntact(Chunk[i]));
                    }
                    if (Got > 0)
                    {
                        Processor.Consume(Chunk[Got - 1].Sequence);
                    }
                    RawCursor.store(Processor.GetCursor(), std::memory_order_release);
                    ++Result.Raw.Reads;
                } while (!IsFinished(Processor.GetCursor()));
            });

        std::thread PendingReader(
            [&]()
            {
                LaneCheck::Scope Lane(Lane::Frame);
                PendingInputReader Reader(Handler);
                InputSlice Pending;
                uint64 Cut = 0;
                do
                {
                    Cut = TickCut.load(std::memory_order_acquire);
                    Reader.Read(Cut, Pending);
                    // While it reads, the writer stays within the limit of the cut before
                    PendingFloor.store(Cut, std::memory_order_release);
                    uint64 Expected = Cut + 1;
                    if (Pending.Overran)
                    {
                        ++Result.Pending.Overruns;
                        Expected = Pending.MouseSamples.empty() ? 0 : Pending.MouseSamples[0].Sequence;
                    }
                    for (const InputMouseSample& Sample : Pending.MouseSamples)
                    {
                        Check(Result.Pending, Expected, Sample.Sequence, IsIntact(Sample));
                    }
                    if (Pending.Cut.AfterSequence != Cut)
                    {
                        ++Result.Pending.OutOfOrder;
                    }
                    ++Result.Pending.Reads;
                } while (!IsFinished(Cut));
            });

        Writer.join();
        TickReader.join();
        RawReader.join();
        PendingReader.join();
        Result.Written = Written.load();
        return Result;
    }

    InputDeviceInfo DeviceInfo(InputDeviceType t_Type, const char* t_Path)
    {
        InputDeviceInfo Info;
        Info.Type = t_Type;
        Info.HardwarePath = t_Path;
        Info.Capabilities = t_Type == InputDeviceType::Mouse
                                ? InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::RelativeMotion
                                : InputDeviceCapabilities::Keys;
        return Info;
    }

    InputMessage Move(InputDeviceId t_Device, int32 t_Dx)
    {
        InputMessage Message;
        Message.Device = t_Device;
        Message.Type = InputMessageType::MouseMove;
        Message.DeltaX = t_Dx;
        return Message;
    }

    std::vector<int32> DeltasOf(const std::vector<InputMouseSample>& t_Samples)
    {
        std::vector<int32> Result;
        for (const InputMouseSample& Sample : t_Samples)
        {
            Result.push_back(Sample.DeltaX);
        }
        return Result;
    }
} // namespace

TEST_CASE("Input queue with threads: one writer and three readers lose, repeat and reorder nothing", "[Input][threads]")
{
    const StressResult Result = RunStress(StressConfig{});
    INFO("written " << Result.Written << ", tick reads " << Result.Tick.Reads << ", pending reads "
                    << Result.Pending.Reads);
    REQUIRE(Result.Written > 10'000);
    for (const ReaderResult* Reader : {&Result.Tick, &Result.Raw})
    {
        REQUIRE(Reader->Messages == Result.Written);
        REQUIRE(Reader->Torn == 0);
        REQUIRE(Reader->OutOfOrder == 0);
        REQUIRE(Reader->Gaps == 0);
        REQUIRE(Reader->Overruns == 0);
    }
    REQUIRE(Result.Pending.Reads > 100);
    REQUIRE(Result.Pending.Torn == 0);
    REQUIRE(Result.Pending.OutOfOrder == 0);
    REQUIRE(Result.Pending.Gaps == 0);
    REQUIRE(Result.Pending.Overruns == 0);
}

TEST_CASE("Input queue with threads: an overrun under load is found, resynced, and never gives a torn message",
          "[Input][threads]")
{
    const StressResult Result = RunStress(StressConfig{.Capacity = 64, .Throttle = false});
    INFO("written " << Result.Written << ", tick gaps " << Result.Tick.Gaps << ", raw overruns "
                    << Result.Raw.Overruns);
    REQUIRE(Result.Written > 10'000);
    // The slow tick reader lost messages, and resynced each time: it ends at the newest message
    REQUIRE(Result.Tick.Gaps > 0);
    REQUIRE(Result.Tick.Messages < Result.Written);
    for (const ReaderResult* Reader : {&Result.Tick, &Result.Raw, &Result.Pending})
    {
        REQUIRE(Reader->Torn == 0);
        REQUIRE(Reader->OutOfOrder == 0);
    }
}

TEST_CASE("Input watermark: with a writer thread, each tick takes exactly the messages of its window",
          "[Input][threads][time]")
{
    // Message i has time i * 997 ns. Tick n covers [n * Window, (n + 1) * Window).
    constexpr uint64 Count = 20'000;
    constexpr int64 Spacing = 997;
    constexpr int64 Window = 50'000;
    // The writer never waits (an overrun drops the oldest messages), and this test does not
    // throttle it. A ring that holds every message keeps a descheduled tick thread from
    // losing messages, so the test checks the windows only. Overruns: the overrun test.
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create(32 * 1024);
    std::atomic<uint64> PublishedCut{0};
    std::atomic<bool> TicksDone{false};
    TickInputBuilder Builder(Handler);

    std::thread Writer(
        [&]()
        {
            for (uint64 i = 1; i <= Count; ++i)
            {
                InputMessage Message = Move(InputDeviceId{1}, static_cast<int32>(i));
                Message.TimeNs = static_cast<int64>(i) * Spacing;
                Handler->HandleMessage(Message);
                // A poll at the time of the next message: everything before it is in
                if (i % 7 == 0 || i == Count)
                {
                    Handler->PollInterfaces(static_cast<int64>(i + 1) * Spacing);
                }
            }
        });

    // The frame side reads after the cut of the tick it was given, at any time
    std::atomic<uint64> PendingErrors{0};
    std::atomic<uint64> PendingReads{0};
    std::thread Frame(
        [&]()
        {
            PendingInputReader Reader(Handler);
            InputSlice Pending;
            while (!TicksDone.load(std::memory_order_acquire))
            {
                const uint64 Cut = PublishedCut.load(std::memory_order_acquire);
                Reader.Read(Cut, Pending);
                if (Pending.Cut.AfterSequence != Cut ||
                    (!Pending.MouseSamples.empty() && Pending.MouseSamples.front().Sequence != Cut + 1))
                {
                    PendingErrors.fetch_add(1);
                }
                for (size_t i = 0; i < Pending.MouseSamples.size(); ++i)
                {
                    if (Pending.MouseSamples[i].Sequence != Cut + 1 + i)
                    {
                        PendingErrors.fetch_add(1);
                    }
                }
                PendingReads.fetch_add(1);
            }
        });

    TickInput Tick;
    uint64 Taken = 0;
    uint64 Wrong = 0;
    const uint64 Ticks = (Count * Spacing) / Window + 1;
    const int64 LastWatermark = static_cast<int64>(Count + 1) * Spacing;
    for (uint64 n = 0; n < Ticks; ++n)
    {
        const int64 Start = static_cast<int64>(n) * Window;
        const int64 End = Start + Window;
        // Wait until every message before the window end is in (the game thread's wait)
        while (Handler->GetWatermark() < std::min(End, LastWatermark))
        {
            std::this_thread::yield();
        }
        Builder.Build(Tick, TickInputLimit{.UntilNs = End});
        for (const InputMouseSample& Sample : Tick.Slice.MouseSamples)
        {
            const int64 Time = static_cast<int64>(Sample.Sequence) * Spacing;
            Wrong += (Time < Start || Time >= End || Sample.Sequence != Taken + 1) ? 1 : 0;
            ++Taken;
        }
        PublishedCut.store(Tick.Slice.Cut.LastSequence, std::memory_order_release);
    }
    TicksDone.store(true, std::memory_order_release);
    Writer.join();
    Frame.join();

    REQUIRE(Wrong == 0);
    REQUIRE(Taken == Count);
    REQUIRE(PendingReads.load() > 0);
    REQUIRE(PendingErrors.load() == 0);
}

TEST_CASE("Input watermark: set by each poll, never goes back", "[Input][time]")
{
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    TickInputBuilder Ticks(Handler);
    REQUIRE(Handler->GetWatermark() == std::numeric_limits<int64>::min());
    Handler->PollInterfaces(500);
    REQUIRE(Handler->GetWatermark() == 500);
    Handler->PollInterfaces(400);
    REQUIRE(Handler->GetWatermark() == 500);

    // A tick may take only messages that the writer finished, before its window end
    for (int64 Time : {100, 200, 300})
    {
        InputMessage Message = Move(InputDeviceId{1}, static_cast<int32>(Time));
        Message.TimeNs = Time;
        Handler->HandleMessage(Message);
    }
    TickInput Tick;
    Ticks.Build(Tick, TickInputLimit{.UntilNs = 300});
    REQUIRE(DeltasOf(Tick.Slice.MouseSamples) == std::vector<int32>{100, 200});
    Ticks.Build(Tick, TickInputLimit{.UntilNs = 301});
    REQUIRE(DeltasOf(Tick.Slice.MouseSamples) == std::vector<int32>{300});
}

TEST_CASE("Input latest snapshot: a copy of the state and its sequence, consistent with the queue", "[Input]")
{
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create(8);
    InputMessage Down;
    Down.Device = InputDeviceId{1};
    Down.Type = InputMessageType::KeyDown;
    Down.Key = Keys::W;
    Handler->HandleMessage(Down);
    for (int i = 0; i < 20; ++i)
    {
        Handler->HandleMessage(Move(InputDeviceId{2}, 1));
    }
    const InputStateSnapshot Latest = Handler->GetLatestSnapshot();
    REQUIRE(Latest.Sequence == 21);
    REQUIRE(Latest.Sequence == Handler->GetLatestSequence());
    REQUIRE(Latest.States.GetKeyState(InputDeviceId{1}, Keys::W).IsHeld);

    // A builder that fell behind resyncs from it: the key stays held, the lost moves are gone
    TickInputBuilder Builder(Handler);
    TickInput Tick;
    for (int i = 0; i < 20; ++i)
    {
        Handler->HandleMessage(Move(InputDeviceId{2}, 1));
    }
    Builder.Build(Tick);
    REQUIRE(Tick.IsHeld(Keys::W));
    REQUIRE(Tick.Slice.Cut.LastSequence == 41);
}

TEST_CASE("UI capture is a message: the tick and the frame side drop the same messages", "[Input][capture]")
{
    const TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();
    const InputInterfaceId Id = Singleton.AddInterface<VirtualInputDeviceInterface>();
    std::shared_ptr<VirtualInputDeviceInterface> Virtual = Singleton.FindInterface<VirtualInputDeviceInterface>(Id);
    const InputDeviceId Mouse = Virtual->CreateDevice(DeviceInfo(InputDeviceType::Mouse, "virtual:mouse"));
    const InputDeviceId Keyboard = Virtual->CreateDevice(DeviceInfo(InputDeviceType::Keyboard, "virtual:kb"));
    Singleton.Poll();

    TickInputBuilder Builder(Singleton.GetHandler());
    PendingInputReader Reader(Singleton.GetHandler());
    const uint64 Start = Singleton.GetHandler()->GetLatestSequence();

    Virtual->Queue(Move(Mouse, 1));
    Singleton.Poll();
    Singleton.SetUICapture(true, true);
    const uint64 CaptureOn = Singleton.GetHandler()->GetLatestSequence();
    Virtual->Queue(Move(Mouse, 2));
    InputMessage Key;
    Key.Device = Keyboard;
    Key.Type = InputMessageType::KeyDown;
    Key.Key = Keys::G;
    Virtual->Queue(Key);
    Singleton.Poll();
    Singleton.SetUICapture(false, false);
    Virtual->Queue(Move(Mouse, 3));
    Singleton.Poll();

    // The capture change is in the stream, with the new state
    InputMessage Change;
    REQUIRE(Singleton.GetHandler()->GetQueue().TryGet(CaptureOn, Change));
    REQUIRE(Change.Type == InputMessageType::CaptureChanged);
    REQUIRE(Change.Capture == (InputCapture::Keyboard | InputCapture::Mouse));

    // The frame side, before the tick: the move and the key inside the capture are dropped
    InputSlice Pending;
    Reader.Read(Start, Pending);
    REQUIRE(DeltasOf(Pending.MouseSamples) == std::vector<int32>{1, 3});
    REQUIRE(Pending.KeyEvents.empty());

    // A tick that ends inside the capture: the same messages dropped, the capture at its cut
    TickInput Tick;
    Builder.Build(Tick, TickInputLimit{.UntilSequence = CaptureOn + 2});
    REQUIRE(DeltasOf(Tick.Slice.MouseSamples) == std::vector<int32>{1});
    REQUIRE(Tick.Slice.KeyEvents.empty());
    REQUIRE_FALSE(Tick.WasPressed(Keys::G));
    REQUIRE(Tick.Capture == (InputCapture::Keyboard | InputCapture::Mouse));

    // The frame after that tick shows exactly the rest, the same way the next tick takes it
    Reader.Read(Tick.Slice.Cut.LastSequence, Pending);
    Builder.Build(Tick);
    REQUIRE(DeltasOf(Pending.MouseSamples) == DeltasOf(Tick.Slice.MouseSamples));
    REQUIRE(DeltasOf(Tick.Slice.MouseSamples) == std::vector<int32>{3});
    REQUIRE(Tick.Capture == InputCapture::None);
    // G was pressed while captured: no press for the game, but it is held now that the capture ended
    REQUIRE(Tick.IsHeld(Keys::G));
    REQUIRE_FALSE(Tick.WasPressed(Keys::G));
    REQUIRE(Tick.Slice.KeyEvents.empty());
}

TEST_CASE("UI capture is recorded and replayed; live UI capture is ignored during a replay",
          "[Input][capture][Recorder]")
{
    const String Path = (std::filesystem::temp_directory_path() / "fling_capture_replay.input.jsonl").string();
    constexpr float Delta = 1.0f / 60.0f;
    std::vector<std::vector<int32>> Live;
    {
        const TestScopes::InputScope Guard;
        Input& Singleton = Input::Get();
        const InputInterfaceId Id = Singleton.AddInterface<VirtualInputDeviceInterface>();
        std::shared_ptr<VirtualInputDeviceInterface> Virtual = Singleton.FindInterface<VirtualInputDeviceInterface>(Id);
        const InputDeviceId Mouse = Virtual->CreateDevice(DeviceInfo(InputDeviceType::Mouse, "virtual:mouse"));
        Singleton.Poll(Delta);
        // A capture that is on when recording starts is in the file too
        Singleton.SetUICapture(false, true);
        Singleton.GetRecorder().Start(Path, 0.0f, 0.0f);
        TickInputProvider Provider(Singleton);
        TickInput Tick;
        for (int Frame = 0; Frame < 12; ++Frame)
        {
            Virtual->Queue(Move(Mouse, Frame + 1));
            if (Frame == 3)
            {
                Singleton.SetUICapture(false, false);
            }
            if (Frame == 8)
            {
                Singleton.SetUICapture(true, true);
            }
            Singleton.Poll(Delta);
            Provider.Build(Tick, TickWindow{});
            Live.push_back(DeltasOf(Tick.Slice.MouseSamples));
        }
        REQUIRE(Singleton.GetRecorder().Stop());
    }
    REQUIRE(Live[0].empty());
    REQUIRE(Live[3] == std::vector<int32>{4});
    REQUIRE(Live[9].empty());

    InputRecording Recording;
    String Error;
    REQUIRE(LoadInputRecording(Path, Recording, Error));
    size_t Changes = 0;
    for (const InputRecordedMessage& Message : Recording.Messages)
    {
        Changes += Message.Message.Type == InputMessageType::CaptureChanged ? 1 : 0;
    }
    REQUIRE(Changes == 3);
    REQUIRE(Recording.TickCuts.size() == 12);
    REQUIRE(WriteInputRecording(Recording).find("\"type\":\"CaptureChanged\",\"keyboard\":true,\"mouse\":true") !=
            String::npos);

    // Replay twice as slow as the recording: the ticks pull their cuts. Live UI capture
    // calls (here: everything captured) change nothing while the file plays.
    {
        const TestScopes::InputScope Guard;
        Input& Singleton = Input::Get();
        String StartError;
        REQUIRE(Singleton.GetPlayback().Start(Recording, 0.0f, StartError));
        TickInputProvider Provider(Singleton);
        TickInput Tick;
        std::vector<std::vector<int32>> Replay;
        for (int Frame = 0; Frame < 12; ++Frame)
        {
            Singleton.SetUICapture(true, true);
            Singleton.Poll(Delta / 2.0f);
            Provider.Build(Tick, TickWindow{});
            Replay.push_back(DeltasOf(Tick.Slice.MouseSamples));
        }
        REQUIRE(Replay == Live);
    }
}

TEST_CASE("A replay starts with no UI capture, whatever the live UI captured when it started",
          "[Input][capture][Playback]")
{
    // Mutation note: fails (every replayed move is dropped as captured) if
    // InputPlayback::BeginPlayback does not queue its start capture.
    const TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();
    Singleton.SetUICapture(false, true);
    REQUIRE(Singleton.IsMouseCaptured());

    // A file with no CaptureChanged, as --record-input writes headless or a person writes by hand
    InputRecording Recording;
    String Error;
    REQUIRE(ParseInputRecording("{\"format\":\"fling-input\",\"version\":2,\"tickRate\":60}\n"
                                "{\"tick\":0,\"type\":\"MouseMove\",\"dx\":3,\"dy\":0}\n"
                                "{\"tick\":2,\"type\":\"MouseMove\",\"dx\":4,\"dy\":0}\n",
                                Recording,
                                Error));
    REQUIRE(Singleton.GetPlayback().Start(Recording, 0.0f, Error));
    TickInputProvider Provider(Singleton);
    TickInput Tick;
    std::vector<int32> Replayed;
    for (int Frame = 0; Frame < 4; ++Frame)
    {
        // The live UI still captures the mouse: ignored while the file plays
        Singleton.SetUICapture(false, true);
        Singleton.Poll(1.0f / 60.0f);
        Provider.Build(Tick, TickWindow{});
        for (const int32 Delta : DeltasOf(Tick.Slice.MouseSamples))
        {
            Replayed.push_back(Delta);
        }
    }
    REQUIRE(Replayed == std::vector<int32>{3, 4});
}

TEST_CASE("TickInputProvider on a tick thread waits for the writer thread to send each replay cut",
          "[Input][threads][Playback]")
{
    // 40 mouse moves over 0.4 s; 30 ticks with cuts that never decrease
    InputRecording Recording;
    InputRecordingDevice Device;
    Device.Id = 1;
    Device.Info = DeviceInfo(InputDeviceType::Mouse, "recorded:mouse");
    Recording.Devices.push_back(Device);
    for (int32 i = 0; i < 40; ++i)
    {
        InputRecordedMessage Message;
        Message.TimeSeconds = 0.01 * i;
        Message.Message = Move(InputDeviceId{1}, i + 1);
        Recording.Messages.push_back(Message);
    }
    for (uint64 k = 1; k <= 30; ++k)
    {
        Recording.TickCuts.push_back(std::min<uint64>(40, (k * 4) / 3));
    }

    const TestScopes::CountLaneViolations Violations;
    const TestScopes::InputScope Guard;
    Input& Singleton = Input::Get();
    Singleton.SetWriterLane(Lane::Frame);
    // Cuts made on ticks at the rate that the playback runs at
    Recording.TickCutRate = Singleton.GetPlayback().GetTickRate();
    String StartError;
    REQUIRE(Singleton.GetPlayback().Start(Recording, 0.0f, StartError));
    TickInputProvider Provider(Singleton, {.CutSender = TickCutSender::Writer, .TickLane = Lane::Game});
    std::atomic<bool> TicksDone{false};

    // The writer polls at its own pace with a slow playback clock (it would take 2.4 s to
    // send everything by time), so most cuts are sent because a tick claimed them
    std::thread Writer(
        [&]()
        {
            LaneCheck::Scope Lane(Lane::Frame);
            while (!TicksDone.load(std::memory_order_acquire))
            {
                Singleton.Poll(1.0f / 600.0f);
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
            Singleton.GetPlayback().Stop();
        });

    std::vector<std::vector<int32>> Ticks;
    std::thread TickThread(
        [&]()
        {
            LaneCheck::Scope Lane(Lane::Game);
            TickInput Tick;
            for (uint64 k = 1; k <= 30; ++k)
            {
                // Not timed: without a cut the tick would take everything in the queue
                Provider.Build(Tick, TickWindow{});
                Ticks.push_back(DeltasOf(Tick.Slice.MouseSamples));
            }
            TicksDone.store(true, std::memory_order_release);
        });
    TickThread.join();
    Writer.join();

    REQUIRE(Ticks.size() == 30);
    uint64 Previous = 0;
    for (size_t k = 0; k < Ticks.size(); ++k)
    {
        INFO("tick " << (k + 1));
        std::vector<int32> Expected;
        for (uint64 i = Previous; i < Recording.TickCuts[k]; ++i)
        {
            Expected.push_back(static_cast<int32>(i + 1));
        }
        REQUIRE(Ticks[k] == Expected);
        Previous = Recording.TickCuts[k];
    }
#if FLING_LANE_CHECKS
    REQUIRE(TestScopes::CountLaneViolations::Count.load() == 0);
#endif
}

#if FLING_LANE_CHECKS
TEST_CASE("Input writer calls on another lane than the writer lane are violations", "[Input][threads]")
{
    const TestScopes::CountLaneViolations Violations;
    std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    Handler->SetWriterLane(Lane::Frame);
    {
        LaneCheck::Scope Lane(Lane::Frame);
        Handler->HandleMessage(Move(InputDeviceId{1}, 1));
    }
    REQUIRE(TestScopes::CountLaneViolations::Count.load() == 0);
    {
        LaneCheck::Scope Lane(Lane::Game);
        Handler->HandleMessage(Move(InputDeviceId{1}, 2));
        Handler->PollInterfaces(0);
        // Readers are fine on any lane
        (void)Handler->GetLatestSnapshot();
        InputSlice Pending;
        PendingInputReader(Handler).Read(0, Pending);
    }
    REQUIRE(TestScopes::CountLaneViolations::Count.load() == 2);
}
#endif

TEST_CASE("CommandLine::Freeze makes it read only", "[CommandLine]")
{
    CommandLine Line;
    const char* Args[] = {"Fling", "--frames=7"};
    REQUIRE(Line.Init(2, Args));
    REQUIRE_FALSE(Line.IsFrozen());
    Line.Freeze();
    REQUIRE(Line.IsFrozen());
    REQUIRE(Line.GetValueAs<int>("frames", 0) == 7);
}
