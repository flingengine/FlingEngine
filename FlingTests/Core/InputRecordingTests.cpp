#include "Input/Input.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputPlayback.h"
#include "Input/InputRecorder.h"
#include "Input/InputRecording.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "Strings/String.h"

#include <catch2/catch_all.hpp>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using Fling::String;
using Fling::StringView;

#if WITH_INPUT_RECORDING
namespace
{
    using namespace Fling;
    using Catch::Matchers::ContainsSubstring;
    using Catch::Matchers::StartsWith;

    constexpr float TickSeconds = 1.0f / 60.0f;

    /** A header line with no devices and a duration of 1 second. */
    const char* const Header = "{\"format\":\"fling-input\",\"version\":1,\"duration\":1.0}\n";

    InputRecording MustParse(const StringView t_Text)
    {
        InputRecording Recording;
        String Error;
        INFO(t_Text);
        REQUIRE(ParseInputRecording(t_Text, Recording, Error));
        REQUIRE(Error.empty());
        return Recording;
    }

    String ParseError(const StringView t_Text)
    {
        InputRecording Recording;
        String Error;
        REQUIRE_FALSE(ParseInputRecording(t_Text, Recording, Error));
        return Error;
    }

    String TempFile(const StringView t_Name)
    {
        const std::filesystem::path Dir = std::filesystem::temp_directory_path() / "FlingInputRecordingTests";
        std::filesystem::create_directories(Dir);
        const std::filesystem::path Path = Dir / t_Name;
        std::filesystem::remove(Path);
        return Path.string();
    }

    InputDeviceInfo KeyboardInfo()
    {
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Keyboard;
        Info.DisplayName = "Test Keyboard";
        Info.HardwarePath = "test:kb";
        Info.Capabilities = InputDeviceCapabilities::Keys;
        return Info;
    }

    InputMessage MakeKey(InputDeviceId t_Device, InputKey t_Key, bool t_Down)
    {
        InputMessage Message;
        Message.Device = t_Device;
        Message.Type = t_Down ? InputMessageType::KeyDown : InputMessageType::KeyUp;
        Message.Key = t_Key;
        return Message;
    }

    /** A handler with one virtual keyboard. */
    struct RecorderFixture
    {
        std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
        std::shared_ptr<VirtualInputDeviceInterface> Interface = std::make_shared<VirtualInputDeviceInterface>();
        InputDeviceId Keyboard;

        RecorderFixture()
        {
            Handler->AddInterface(Interface);
            Keyboard = Interface->CreateDevice(KeyboardInfo());
        }

        void Send(InputKey t_Key, bool t_Down)
        {
            Interface->Queue(MakeKey(Keyboard, t_Key, t_Down));
            Handler->PollInterfaces(PlatformTime::NowNanoseconds());
        }
    };

    bool IsHeldAnywhere(const InputMessageProcessor& t_Processor, InputKey t_Key)
    {
        for (const auto& Entry : t_Processor.GetState().GetDevices())
        {
            if (Entry.second.GetKeyState(t_Key).IsHeld)
            {
                return true;
            }
        }
        return false;
    }
} // namespace

TEST_CASE("InputRecording parses the documented example", "[Input][Recording]")
{
    const String Text = "{\"format\":\"fling-input\",\"version\":1,\"duration\":4.0}\n"
                        "# Walk forward for one second, then strafe left\n"
                        "{\"t\":0.0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"
                        "{\"t\":1.0,\"type\":\"KeyUp\",\"key\":\"W\"}\n"
                        "{\"t\":1.0,\"type\":\"Press\",\"key\":\"A\",\"duration\":0.5}\n"
                        "\n"
                        "{\"t\":2.0,\"type\":\"Axis\",\"key\":\"GamepadRightX\",\"value\":1.0}\n"
                        "{\"t\":2.5,\"type\":\"Axis\",\"key\":\"GamepadRightX\",\"value\":0.0}\n"
                        "{\"tick\":200,\"type\":\"Press\",\"key\":\"Escape\"}\n";
    const InputRecording Recording = MustParse(Text);

    CHECK(Recording.Version == 1);
    CHECK(Recording.TickRate == 60);
    CHECK(Recording.GetLength() == Catch::Approx(4.0));
    REQUIRE(Recording.Devices.size() == 2);
    CHECK(Recording.Devices[0].Info.Type == InputDeviceType::Keyboard);
    CHECK(Recording.Devices[1].Info.Type == InputDeviceType::Gamepad);
    CHECK(Recording.Devices[0].Id != Recording.Devices[1].Id);
    REQUIRE(Recording.Messages.size() == 8);

    // Messages at the same time keep their order in the file.
    CHECK(Recording.Messages[1].Message.Type == InputMessageType::KeyUp);
    CHECK(Recording.Messages[1].Message.Key == Keys::W);
    CHECK(Recording.Messages[2].Message.Type == InputMessageType::KeyDown);
    CHECK(Recording.Messages[2].Message.Key == Keys::A);
    CHECK(Recording.Messages[3].TimeSeconds == Catch::Approx(1.5));
    CHECK(Recording.Messages[3].Message.Type == InputMessageType::KeyUp);
    CHECK(Recording.Messages[4].Message.Value == Catch::Approx(1.0f));

    // 200 ticks at 60 per second.
    CHECK(Recording.Messages[6].TimeSeconds == Catch::Approx(200.0 / 60.0));
    CHECK(Recording.Messages[6].Message.Key == Keys::Escape);
    CHECK(Recording.Messages[7].Message.Type == InputMessageType::KeyUp);
}

TEST_CASE("InputRecording picks a device for a message", "[Input][Recording]")
{
    SECTION("No devices listed: devices come from the keys")
    {
        const InputRecording Recording =
            MustParse(String(Header) + "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"
                                       "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"MouseLeft\"}\n"
                                       "{\"t\":0,\"type\":\"MouseMove\",\"dx\":1}\n"
                                       "{\"t\":0,\"type\":\"Char\",\"codepoint\":65}\n"
                                       "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"GamepadFaceBottom\"}\n");
        REQUIRE(Recording.Devices.size() == 3);
        CHECK(Recording.Devices[0].Info.Type == InputDeviceType::Keyboard);
        CHECK(Recording.Devices[1].Info.Type == InputDeviceType::Mouse);
        CHECK(Recording.Devices[2].Info.Type == InputDeviceType::Gamepad);
        CHECK(Recording.Devices[0].Info.DisplayName == "Keyboard");
        CHECK(Recording.Devices[0].Info.HasCapability(InputDeviceCapabilities::Keys));
        CHECK(Recording.Devices[1].Info.HasCapability(InputDeviceCapabilities::Pointer));
        CHECK(Recording.Devices[2].Info.HasCapability(InputDeviceCapabilities::Thumbsticks));
        CHECK(Recording.Messages[1].Message.Device.Value == Recording.Devices[1].Id);
        CHECK(Recording.Messages[2].Message.Device.Value == Recording.Devices[1].Id);
        CHECK(Recording.Messages[3].Message.Device.Value == Recording.Devices[0].Id);
    }

    SECTION("A device type name uses the first listed device of that type")
    {
        const InputRecording Recording = MustParse(
            "{\"format\":\"fling-input\",\"version\":1,\"devices\":[{\"id\":7,\"type\":\"Gamepad\",\"name\":\"Pad\","
            "\"vendorId\":1118,\"productId\":2834,\"capabilities\":[\"Buttons\",\"Haptics\"]}]}\n"
            "{\"t\":0,\"device\":\"Gamepad\",\"type\":\"KeyDown\",\"key\":\"GamepadFaceBottom\"}\n"
            "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n");
        REQUIRE(Recording.Devices.size() == 2);
        CHECK(Recording.Messages[0].Message.Device.Value == 7);
        CHECK(Recording.Devices[0].Info.VendorId == 1118);
        CHECK(Recording.Devices[0].Info.ProductId == 2834);
        CHECK(Recording.Devices[0].Info.HasCapability(InputDeviceCapabilities::Haptics));
        CHECK_FALSE(Recording.Devices[0].Info.HasCapability(InputDeviceCapabilities::Triggers));
        // An inferred device gets the next free ID.
        CHECK(Recording.Devices[1].Id == 8);
    }

    SECTION("A device ID that is not in the header is an error")
    {
        const String Error = ParseError(String(Header) + "{\"t\":0,\"device\":5,\"type\":\"KeyDown\",\"key\":\"W\"}\n");
        CHECK_THAT(Error, StartsWith("line 2:"));
        CHECK_THAT(Error, ContainsSubstring("5"));
    }

    SECTION("An unknown device type name is an error")
    {
        const String Error =
            ParseError(String(Header) + "{\"t\":0,\"device\":\"Toaster\",\"type\":\"KeyDown\",\"key\":\"W\"}\n");
        CHECK_THAT(Error, StartsWith("line 2:"));
    }
}

TEST_CASE("InputRecording expands Press", "[Input][Recording]")
{
    SECTION("With a duration")
    {
        const InputRecording Recording =
            MustParse(String(Header) + "{\"t\":1.0,\"type\":\"Press\",\"key\":\"Space\",\"duration\":0.25}\n");
        REQUIRE(Recording.Messages.size() == 2);
        CHECK(Recording.Messages[0].Message.Type == InputMessageType::KeyDown);
        CHECK(Recording.Messages[0].TimeSeconds == Catch::Approx(1.0));
        CHECK(Recording.Messages[1].Message.Type == InputMessageType::KeyUp);
        CHECK(Recording.Messages[1].TimeSeconds == Catch::Approx(1.25));
    }

    SECTION("With no duration, both happen at the same time")
    {
        const InputRecording Recording =
            MustParse(String(Header) + "{\"t\":1.0,\"type\":\"Press\",\"key\":\"Space\"}\n");
        REQUIRE(Recording.Messages.size() == 2);
        CHECK(Recording.Messages[0].Message.Type == InputMessageType::KeyDown);
        CHECK(Recording.Messages[1].Message.Type == InputMessageType::KeyUp);
        CHECK(Recording.Messages[0].TimeSeconds == Recording.Messages[1].TimeSeconds);
    }

    SECTION("With ticks")
    {
        const InputRecording Recording = MustParse("{\"format\":\"fling-input\",\"version\":1,\"tickRate\":30}\n"
                                                   "{\"tick\":30,\"type\":\"Press\",\"key\":\"Space\",\"ticks\":15}\n");
        REQUIRE(Recording.Messages.size() == 2);
        CHECK(Recording.Messages[0].TimeSeconds == Catch::Approx(1.0));
        CHECK(Recording.Messages[1].TimeSeconds == Catch::Approx(1.5));
    }
}

TEST_CASE("InputRecording expands text into Char messages", "[Input][Recording]")
{
    const InputRecording Recording =
        MustParse(String(Header) + "{\"t\":0.5,\"type\":\"Char\",\"text\":\"h\xC3\xA9y\"}\n");
    REQUIRE(Recording.Messages.size() == 3);
    CHECK(Recording.Messages[0].Message.CodePoint == 0x68);
    CHECK(Recording.Messages[1].Message.CodePoint == 0xE9);
    CHECK(Recording.Messages[2].Message.CodePoint == 0x79);
    CHECK(Recording.Messages[2].Message.Type == InputMessageType::Char);
}

TEST_CASE("InputRecording gives mouse moves a position", "[Input][Recording]")
{
    const InputRecording Recording =
        MustParse(String(Header) + "{\"t\":0.1,\"type\":\"MouseMove\",\"dx\":3,\"dy\":4}\n"
                                   "{\"t\":0.2,\"type\":\"MouseMove\",\"dx\":1,\"dy\":1}\n"
                                   "{\"t\":0.3,\"type\":\"MouseMove\",\"dx\":1,\"dy\":1,\"x\":100,\"y\":50}\n");
    REQUIRE(Recording.Messages.size() == 3);
    CHECK(Recording.Messages[0].Message.PositionX == Catch::Approx(3.0f));
    CHECK(Recording.Messages[0].Message.PositionY == Catch::Approx(4.0f));
    CHECK(Recording.Messages[1].Message.PositionX == Catch::Approx(4.0f));
    CHECK(Recording.Messages[1].Message.PositionY == Catch::Approx(5.0f));
    CHECK(Recording.Messages[2].Message.PositionX == Catch::Approx(100.0f));
}

TEST_CASE("InputRecording skips comments, blank lines, and unknown fields", "[Input][Recording]")
{
    const InputRecording Recording =
        MustParse("\n# a comment\n{\"format\":\"fling-input\",\"version\":1,\"extra\":[1,2]}\r\n"
                  "   \n  # indented comment\n"
                  "{\"t\":0.5,\"type\":\"KeyDown\",\"key\":\"W\",\"note\":\"hello\"}\r\n");
    CHECK(Recording.Messages.size() == 1);
}

TEST_CASE("InputRecording errors name the line", "[Input][Recording]")
{
    SECTION("No header")
    {
        CHECK_THAT(ParseError(""), StartsWith("line 1:"));
    }

    SECTION("A bad header")
    {
        CHECK_THAT(ParseError("{\"format\":\"other\",\"version\":1}\n"), StartsWith("line 1:"));
        CHECK_THAT(ParseError("# comment\n{\"format\":\"fling-input\"}\n"), StartsWith("line 2:"));
        // Version 2 (tick cuts with their rate) is the newest; there is no version 3 yet
        CHECK_THAT(ParseError("{\"format\":\"fling-input\",\"version\":3}\n"), StartsWith("line 1:"));
        CHECK_THAT(ParseError("not json\n"), StartsWith("line 1:"));
    }

    SECTION("An unknown key")
    {
        const String Error =
            ParseError(String(Header) + "# comment\n{\"t\":0,\"type\":\"KeyDown\",\"key\":\"NoSuchKey\"}\n");
        CHECK_THAT(Error, StartsWith("line 3:"));
        CHECK_THAT(Error, ContainsSubstring("NoSuchKey"));
    }

    SECTION("An unknown message type")
    {
        CHECK_THAT(ParseError(String(Header) + "{\"t\":0,\"type\":\"Explode\"}\n"), StartsWith("line 2:"));
    }

    SECTION("Both t and tick")
    {
        CHECK_THAT(ParseError(String(Header) + "\n{\"t\":0,\"tick\":1,\"type\":\"KeyDown\",\"key\":\"W\"}\n"),
                   StartsWith("line 3:"));
    }

    SECTION("Neither t nor tick")
    {
        CHECK_THAT(ParseError(String(Header) + "{\"type\":\"KeyDown\",\"key\":\"W\"}\n"), StartsWith("line 2:"));
    }

    SECTION("A key that is not an axis in an Axis message")
    {
        CHECK_THAT(ParseError(String(Header) + "{\"t\":0,\"type\":\"Axis\",\"key\":\"W\",\"value\":1}\n"),
                   StartsWith("line 2:"));
    }

    SECTION("An Axis message with no value")
    {
        CHECK_THAT(ParseError(String(Header) + "{\"t\":0,\"type\":\"Axis\",\"key\":\"GamepadLeftX\"}\n"),
                   StartsWith("line 2:"));
    }

    SECTION("A message line that is not valid JSON")
    {
        CHECK_THAT(ParseError(String(Header) + "{\"t\":0,\n"), StartsWith("line 2:"));
    }

    SECTION("A file that is missing")
    {
        InputRecording Recording;
        String Error;
        CHECK_FALSE(LoadInputRecording(TempFile("does_not_exist.input.jsonl"), Recording, Error));
        CHECK_FALSE(Error.empty());
    }
}

TEST_CASE("InputRecording sorts messages by time", "[Input][Recording]")
{
    const InputRecording Recording = MustParse(String(Header) + "{\"t\":2,\"type\":\"KeyUp\",\"key\":\"W\"}\n"
                                                                "{\"t\":1,\"type\":\"KeyDown\",\"key\":\"W\"}\n");
    REQUIRE(Recording.Messages.size() == 2);
    CHECK(Recording.Messages[0].Message.Type == InputMessageType::KeyDown);
    CHECK(Recording.Messages[1].Message.Type == InputMessageType::KeyUp);
}

TEST_CASE("InputRecording write and parse round trip", "[Input][Recording]")
{
    InputRecording Recording;
    Recording.Duration = 3.5;
    InputRecordingDevice Keyboard;
    Keyboard.Id = 4;
    Keyboard.Info = KeyboardInfo();
    InputRecordingDevice Pad;
    Pad.Id = 9;
    Pad.Info.Type = InputDeviceType::Gamepad;
    Pad.Info.DisplayName = "Pad \"One\"";
    Pad.Info.VendorId = 1118;
    Pad.Info.ProductId = 2834;
    Pad.Info.HardwarePath = "glfw:abc";
    Pad.Info.Capabilities = InputDeviceCapabilities::Buttons | InputDeviceCapabilities::Gyro;
    Recording.Devices = {Keyboard, Pad};

    const auto Add = [&Recording](double t_Time, InputMessage t_Message)
    {
        InputRecordedMessage Item;
        Item.TimeSeconds = t_Time;
        Item.Message = t_Message;
        Recording.Messages.push_back(Item);
    };
    InputMessage Down = MakeKey(InputDeviceId{4}, Keys::W, true);
    Down.Scancode = 25;
    Add(0.0, Down);
    InputMessage Axis;
    Axis.Device = InputDeviceId{9};
    Axis.Type = InputMessageType::Axis;
    Axis.Key = Keys::GamepadLeftX;
    Axis.Value = 0.1f;
    Add(0.016667, Axis);
    InputMessage Move;
    Move.Device = InputDeviceId{4};
    Move.Type = InputMessageType::MouseMove;
    Move.DeltaX = -3;
    Move.DeltaY = 7;
    Move.PositionX = 12.5f;
    Move.PositionY = 99.0f;
    Add(1.234567, Move);
    InputMessage Wheel;
    Wheel.Device = InputDeviceId{4};
    Wheel.Type = InputMessageType::MouseWheel;
    Wheel.WheelY = -1.0f;
    Add(1.5, Wheel);
    InputMessage Char;
    Char.Device = InputDeviceId{4};
    Char.Type = InputMessageType::Char;
    Char.CodePoint = 0x20AC;
    Add(2.0, Char);
    InputMessage Focus;
    Focus.Device = InputDeviceId{4};
    Focus.Type = InputMessageType::FocusLost;
    Add(2.5, Focus);
    Add(3.0, MakeKey(InputDeviceId{4}, Keys::W, false));

    const String Text = WriteInputRecording(Recording);
    const InputRecording Parsed = MustParse(Text);

    CHECK(Parsed.Duration == Catch::Approx(3.5));
    REQUIRE(Parsed.Devices.size() == 2);
    CHECK(Parsed.Devices[1].Id == 9);
    CHECK(Parsed.Devices[1].Info.DisplayName == "Pad \"One\"");
    CHECK(Parsed.Devices[1].Info.VendorId == 1118);
    CHECK(Parsed.Devices[1].Info.HardwarePath == "glfw:abc");
    CHECK(Parsed.Devices[1].Info.Capabilities == Pad.Info.Capabilities);
    CHECK(Parsed.Devices[0].Info.Capabilities == Keyboard.Info.Capabilities);

    REQUIRE(Parsed.Messages.size() == Recording.Messages.size());
    for (size_t i = 0; i < Recording.Messages.size(); ++i)
    {
        const InputMessage& A = Recording.Messages[i].Message;
        const InputMessage& B = Parsed.Messages[i].Message;
        INFO("message " << i);
        CHECK(Parsed.Messages[i].TimeSeconds == Catch::Approx(Recording.Messages[i].TimeSeconds).margin(1e-7));
        CHECK(A.Type == B.Type);
        CHECK(A.Device == B.Device);
        CHECK(A.Key == B.Key);
        CHECK(A.Scancode == B.Scancode);
        CHECK(A.CodePoint == B.CodePoint);
        CHECK(A.Value == B.Value);
        CHECK(A.DeltaX == B.DeltaX);
        CHECK(A.DeltaY == B.DeltaY);
        CHECK(A.PositionX == B.PositionX);
        CHECK(A.PositionY == B.PositionY);
        CHECK(A.WheelX == B.WheelX);
        CHECK(A.WheelY == B.WheelY);
    }

    // One line for the header and one for each message.
    CHECK(std::count(Text.begin(), Text.end(), '\n') == static_cast<long>(1 + Recording.Messages.size()));
}

TEST_CASE("Input recordings keep per-tick cuts, and old files load without them", "[time][InputRecording]")
{
    InputRecording Recording;
    InputRecordingDevice Device;
    Device.Id = 1;
    Device.Info.Type = InputDeviceType::Keyboard;
    Recording.Devices.push_back(Device);
    for (int i = 0; i < 3; ++i)
    {
        InputRecordedMessage Message;
        Message.TimeSeconds = 0.1 * i;
        Message.Message.Device = InputDeviceId{1};
        Message.Message.Type = InputMessageType::KeyDown;
        Message.Message.Key = Keys::A;
        Recording.Messages.push_back(Message);
    }
    Recording.TickCuts = {0, 1, 1, 3, 3};
    Recording.TickCutRate = 30;

    // Version 2: the cuts keep the rate of the ticks they were recorded on
    const String Text = WriteInputRecording(Recording);
    REQUIRE(Text.find("\"version\":2") != String::npos);
    REQUIRE(Text.find("{\"tickCuts\":{\"tickRate\":30,\"counts\":[0,1,1,3,3]}}") != String::npos);
    InputRecording Read;
    String Error;
    REQUIRE(ParseInputRecording(Text, Read, Error));
    REQUIRE(Error.empty());
    REQUIRE(Read.Version == 2);
    REQUIRE(Read.Messages.size() == 3);
    REQUIRE(Read.TickCuts == std::vector<uint64>{0, 1, 1, 3, 3});
    REQUIRE(Read.TickCutRate == 30);

    // A file with no cuts line loads, and replays by time
    InputRecording Old = Recording;
    Old.TickCuts.clear();
    REQUIRE(WriteInputRecording(Old).find("tickCuts") == String::npos);
    REQUIRE(ParseInputRecording(WriteInputRecording(Old), Read, Error));
    REQUIRE(Read.TickCuts.empty());
    REQUIRE(Read.TickCutRate == 0);

    // A version 1 file stored the cuts as a bare list, with no rate: they were recorded at 60 Hz
    const String Version1 = "{\"format\":\"fling-input\",\"version\":1}\n"
                            "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"A\"}\n"
                            "{\"t\":0.1,\"type\":\"KeyDown\",\"key\":\"B\"}\n"
                            "{\"tickCuts\":[0,1,2]}\n";
    REQUIRE(ParseInputRecording(Version1, Read, Error));
    REQUIRE(Read.Version == 1);
    REQUIRE(Read.TickCuts == std::vector<uint64>{0, 1, 2});
    REQUIRE(Read.TickCutRate == InputRecording::Version1TickCutRate);
    REQUIRE(Read.TickCutRate == 60);
    // Written again, it is a version 2 file that says 60 Hz
    REQUIRE(WriteInputRecording(Read).find("{\"tickCuts\":{\"tickRate\":60,") != String::npos);

    // Bad cuts
    REQUIRE_FALSE(ParseInputRecording(Text + "{\"tickCuts\":{\"tickRate\":30,\"counts\":[2,1]}}\n", Read, Error));
    REQUIRE(Error.find("never decrease") != String::npos);
    REQUIRE_FALSE(ParseInputRecording(
        WriteInputRecording(Old) + "{\"tickCuts\":{\"tickRate\":30,\"counts\":[0,4]}}\n", Read, Error));
    REQUIRE(Error.find("more messages") != String::npos);
    REQUIRE_FALSE(ParseInputRecording(WriteInputRecording(Old) + "{\"tickCuts\":[0,1]}\n", Read, Error));
    REQUIRE(Error.find("must be an object") != String::npos);
    REQUIRE_FALSE(ParseInputRecording(WriteInputRecording(Old) + "{\"tickCuts\":{\"counts\":[0,1]}}\n", Read, Error));
    REQUIRE(Error.find("needs a \"tickRate\"") != String::npos);
    REQUIRE_FALSE(ParseInputRecording(
        WriteInputRecording(Old) + "{\"tickCuts\":{\"tickRate\":0,\"counts\":[0]}}\n", Read, Error));
    REQUIRE_FALSE(ParseInputRecording(Text + "{\"tickCuts\":{\"tickRate\":60,\"counts\":[3]}}\n", Read, Error));
    REQUIRE(Error.find("same \"tickRate\"") != String::npos);
    REQUIRE_FALSE(ParseInputRecording("{\"format\":\"fling-input\",\"version\":3}\n", Read, Error));
    REQUIRE(Error.find("unsupported version 3") != String::npos);
}

TEST_CASE("Input recordings with tick cuts replay only at the rate they were recorded at", "[time][InputRecording]")
{
    InputRecording Recording;
    String Error;
    REQUIRE(ParseInputRecording("{\"format\":\"fling-input\",\"version\":1}\n"
                                "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"A\"}\n"
                                "{\"tickCuts\":[0,1]}\n",
                                Recording,
                                Error));

    // A version 1 file: 60 Hz
    REQUIRE(CheckTickCutRate(Recording, 60, Error));
    REQUIRE_FALSE(CheckTickCutRate(Recording, 30, Error));
    REQUIRE(Error == "its tick cuts were recorded at 60 ticks a second, and the simulation runs at 30. Replay it with "
                     "--sim-hz 60 (or [Engine] SimHz=60)");

    // A file with no cuts replays by time at any rate
    Recording.TickCuts.clear();
    REQUIRE(CheckTickCutRate(Recording, 30, Error));
    REQUIRE(CheckTickCutRate(Recording, 144, Error));

    // Playback refuses the mismatch and plays nothing; at the recorded rate it plays
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputPlayback Playback(Handler);
    REQUIRE(Playback.GetTickRate() == 30);
    Recording.TickCuts = {0, 1};
    Recording.TickCutRate = 60;
    REQUIRE_FALSE(Playback.Start(Recording, 0.0f, Error));
    REQUIRE(Error.find("--sim-hz 60") != String::npos);
    REQUIRE_FALSE(Playback.IsActive());
    Playback.SetTickRate(60);
    REQUIRE(Playback.Start(Recording, 0.0f, Error));
    REQUIRE(Playback.HasTickCuts());
    Playback.Stop();

    // From a file, the message names the file
    const String Path = TempFile("cuts_at_60.input.jsonl");
    REQUIRE(SaveInputRecording(Path, Recording, Error));
    Playback.SetTickRate(30);
    REQUIRE_FALSE(Playback.Start(Path, 0.0f, Error));
    REQUIRE(Error.find("cuts_at_60.input.jsonl") != String::npos);
    REQUIRE(Error.find("recorded at 60 ticks a second") != String::npos);
}

TEST_CASE("The recorder stores the tick rate with its cuts", "[time][InputRecording]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    for (const uint32 Hz : {30u, 60u})
    {
        CAPTURE(Hz);
        const String Path = TempFile("recorder_rate.input.jsonl");
        InputRecorder Recorder(Handler);
        Recorder.SetTickRate(Hz);
        Recorder.Start(Path, 0.0f, 0.0f);
        Recorder.Update(0.0f);
        Recorder.NoteTickCut(InputCut{});
        Recorder.NoteTickCut(InputCut{});
        REQUIRE(Recorder.Stop());

        InputRecording Read;
        String Error;
        REQUIRE(LoadInputRecording(Path, Read, Error));
        REQUIRE(Read.TickCuts.size() == 2);
        REQUIRE(Read.TickCutRate == Hz);
        REQUIRE(Read.TickRate == Hz);

        // It replays at its own rate only
        InputPlayback Playback(Handler);
        Playback.SetTickRate(Hz);
        REQUIRE(Playback.Start(Path, 0.0f, Error));
        Playback.Stop();
        Playback.SetTickRate(Hz == 30 ? 60 : 30);
        REQUIRE_FALSE(Playback.Start(Path, 0.0f, Error));
    }
}

TEST_CASE("InputRecording saves and loads a file", "[Input][Recording]")
{
    const String Path = TempFile("nested/folder/save_load.input.jsonl");
    InputRecording Recording = MustParse(String(Header) + "{\"t\":0.5,\"type\":\"KeyDown\",\"key\":\"W\"}\n");
    String Error;
    REQUIRE(SaveInputRecording(Path, Recording, Error));
    InputRecording Loaded;
    REQUIRE(LoadInputRecording(Path, Loaded, Error));
    CHECK(Loaded.Messages.size() == 1);
}

TEST_CASE("DescribeRecordedMessage gives one readable line", "[Input][Recording]")
{
    CHECK(DescribeRecordedMessage(1.25, "Keyboard", MakeKey(InputDeviceId{1}, Keys::W, true)) ==
          "1.250s  Keyboard  KeyDown W");

    InputMessage Axis;
    Axis.Type = InputMessageType::Axis;
    Axis.Key = Keys::GamepadLeftX;
    Axis.Value = 0.5f;
    CHECK_THAT(DescribeRecordedMessage(0.0, "Pad", Axis), ContainsSubstring("GamepadLeftX 0.500"));

    InputMessage Move;
    Move.Type = InputMessageType::MouseMove;
    Move.DeltaX = 2;
    Move.DeltaY = -3;
    CHECK_THAT(DescribeRecordedMessage(0.0, "Mouse", Move), ContainsSubstring("dx=2 dy=-3"));
}

TEST_CASE("InputEventLog keeps the newest lines", "[Input][Recording]")
{
    InputEventLog Log(3);
    for (int i = 0; i < 5; ++i)
    {
        Log.Add(std::to_string(i));
    }
    REQUIRE(Log.GetLines().size() == 3);
    CHECK(Log.GetLines().front() == "2");
    CHECK(Log.GetLines().back() == "4");
    CHECK(Log.GetTotalAdded() == 5);
    Log.Clear();
    CHECK(Log.GetLines().empty());
    CHECK(Log.GetTotalAdded() == 0);
}

TEST_CASE("InputRecorder records messages to a file", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    InputRecorder Recorder(Fixture.Handler);
    const String Path = TempFile("recorder_basic.input.jsonl");

    Recorder.Start(Path, 0.0f, 0.0f);
    REQUIRE(Recorder.GetState() == InputRecorderState::Recording);

    Fixture.Send(Keys::W, true);
    Recorder.Update(0.1f);
    Fixture.Send(Keys::W, false);
    Recorder.Update(0.1f);
    // The start capture (none), then the two keys
    CHECK(Recorder.GetMessageCount() == 3);

    REQUIRE(Recorder.Stop());
    CHECK(Recorder.GetState() == InputRecorderState::Idle);
    CHECK(Recorder.GetSaveCount() == 1);
    CHECK(Recorder.GetLastSavedPath() == Path);
    CHECK(Recorder.GetLastError().empty());
    CHECK_FALSE(Recorder.GetLog().GetLines().empty());

    InputRecording Loaded;
    String Error;
    REQUIRE(LoadInputRecording(Path, Loaded, Error));
    REQUIRE(Loaded.Messages.size() == 3);
    CHECK(Loaded.Messages[0].Message.Type == InputMessageType::CaptureChanged);
    CHECK(Loaded.Messages[0].Message.Capture == InputCapture::None);
    CHECK(Loaded.Messages[1].Message.Type == InputMessageType::KeyDown);
    CHECK(Loaded.Messages[1].Message.Key == Keys::W);
    CHECK(Loaded.Messages[2].Message.Type == InputMessageType::KeyUp);
    CHECK(Loaded.Messages[2].TimeSeconds >= Loaded.Messages[1].TimeSeconds);
    CHECK(Loaded.Duration == Catch::Approx(0.2).margin(1e-6));
    REQUIRE(Loaded.Devices.size() == 1);
    CHECK(Loaded.Devices[0].Id == Fixture.Keyboard.Value);
    CHECK(Loaded.Devices[0].Info.DisplayName == "Test Keyboard");
    CHECK(Loaded.Devices[0].Info.Type == InputDeviceType::Keyboard);
}

TEST_CASE("InputRecorder does not record messages from before it started", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    Fixture.Send(Keys::A, true);
    Fixture.Send(Keys::A, false);

    InputRecorder Recorder(Fixture.Handler);
    Recorder.Start(TempFile("recorder_before.input.jsonl"), 0.0f, 0.0f);
    Recorder.Update(0.1f);
    // Only the start capture
    CHECK(Recorder.GetMessageCount() == 1);
    REQUIRE(Recorder.Stop());
}

TEST_CASE("InputRecorder writes held keys at time 0", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    Fixture.Send(Keys::W, true);

    InputRecorder Recorder(Fixture.Handler);
    const String Path = TempFile("recorder_held.input.jsonl");
    Recorder.Start(Path, 0.0f, 0.0f);
    REQUIRE(Recorder.Stop());

    InputRecording Loaded;
    String Error;
    REQUIRE(LoadInputRecording(Path, Loaded, Error));
    REQUIRE(Loaded.Messages.size() == 2);
    CHECK(Loaded.Messages[0].Message.Type == InputMessageType::KeyDown);
    CHECK(Loaded.Messages[0].Message.Key == Keys::W);
    CHECK(Loaded.Messages[0].TimeSeconds == 0.0);
    CHECK(Loaded.Messages[1].Message.Type == InputMessageType::CaptureChanged);
    CHECK(Loaded.Messages[1].TimeSeconds == 0.0);
}

TEST_CASE("InputRecorder counts down before it records", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    InputRecorder Recorder(Fixture.Handler);
    const String Path = TempFile("recorder_countdown.input.jsonl");

    Recorder.Start(Path, 0.5f, 0.0f);
    CHECK(Recorder.GetState() == InputRecorderState::Countdown);
    CHECK(Recorder.GetCountdownTotal() == Catch::Approx(0.5f));

    Fixture.Send(Keys::W, true);
    Recorder.Update(0.25f);
    CHECK(Recorder.GetState() == InputRecorderState::Countdown);
    CHECK(Recorder.GetCountdownRemaining() == Catch::Approx(0.25f));
    Recorder.Update(0.25f);
    CHECK(Recorder.GetState() == InputRecorderState::Recording);

    SECTION("Stop during the countdown writes nothing")
    {
        InputRecorder Other(Fixture.Handler);
        const String OtherPath = TempFile("recorder_cancelled.input.jsonl");
        Other.Start(OtherPath, 5.0f, 0.0f);
        CHECK(Other.Stop());
        CHECK(Other.GetState() == InputRecorderState::Idle);
        CHECK(Other.GetSaveCount() == 0);
        CHECK_FALSE(std::filesystem::exists(OtherPath));
    }
    REQUIRE(Recorder.Stop());
}

TEST_CASE("InputRecorder stops by itself at the duration", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    InputRecorder Recorder(Fixture.Handler);
    const String Path = TempFile("recorder_duration.input.jsonl");

    Recorder.Start(Path, 0.0f, 0.1f);
    Fixture.Send(Keys::W, true);
    Recorder.Update(0.06f);
    CHECK(Recorder.GetState() == InputRecorderState::Recording);
    Recorder.Update(0.06f);
    CHECK(Recorder.GetState() == InputRecorderState::Idle);
    CHECK(Recorder.GetSaveCount() == 1);
    CHECK(std::filesystem::exists(Path));
}

TEST_CASE("InputRecorder reports a save that fails", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    InputRecorder Recorder(Fixture.Handler);
    // A folder cannot be written as a file.
    const String Path = std::filesystem::temp_directory_path().string();
    Recorder.Start(Path, 0.0f, 0.0f);
    CHECK_FALSE(Recorder.Stop());
    CHECK_FALSE(Recorder.GetLastError().empty());
    CHECK(Recorder.GetSaveCount() == 0);
}

TEST_CASE("InputPlayback sends each message on the right update", "[Input][Playback]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputMessageProcessor Processor = Handler->CreateProcessor();
    InputPlayback Playback(Handler);

    String StartError;
    REQUIRE(Playback.Start(MustParse("{\"format\":\"fling-input\",\"version\":1,\"duration\":1.0}\n"
                                     "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"A\"}\n"
                                     "{\"t\":0.5,\"type\":\"KeyDown\",\"key\":\"W\"}\n"),
                           0.0f,
                           StartError));
    REQUIRE(Playback.GetState() == InputPlaybackState::Playing);
    CHECK(Handler->FindInterface(InputPlayback::InterfaceName) != nullptr);

    int FinishedAt = -1;
    for (int Update = 0; Update < 100; ++Update)
    {
        Playback.Update(TickSeconds);
        Handler->PollInterfaces(PlatformTime::NowNanoseconds());
        Processor.ConsumeUntil();

        // A message at time 0 goes out on update 0, and one at 0.5 s on update 30.
        if (Playback.IsActive())
        {
            CHECK(IsHeldAnywhere(Processor, Keys::A));
            CHECK(IsHeldAnywhere(Processor, Keys::W) == (Update >= 30));
        }
        if (!Playback.IsActive())
        {
            FinishedAt = Update;
            break;
        }
    }
    REQUIRE(FinishedAt > 30);
    CHECK(Playback.GetFinishedCount() == 1);
    CHECK(Playback.GetSentCount() == 2);

    // At the end the interface goes away, and the held keys are released.
    CHECK(Handler->FindInterface(InputPlayback::InterfaceName) == nullptr);
    CHECK_FALSE(IsHeldAnywhere(Processor, Keys::A));
    CHECK_FALSE(IsHeldAnywhere(Processor, Keys::W));
    CHECK(Handler->GetDeviceFactory().GetConnectedDevices().empty());
}

TEST_CASE("A replay that overruns the input queue stops with an error", "[Input][Playback]")
{
    // Mutation note: fails (the replay goes on with lost input and reports nothing) if
    // TickInputProvider::Build does not call InputPlayback::NoteTickOverran.
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create(8);
    InputPlayback Playback(Handler);
    TickInputProvider Provider(Handler, &Playback, nullptr);
    String Text = "{\"format\":\"fling-input\",\"version\":2,\"duration\":1.0}\n";
    for (int i = 0; i < 20; ++i)
    {
        Text += "{\"t\":0,\"type\":\"MouseMove\",\"dx\":1,\"dy\":0}\n";
    }
    String Error;
    REQUIRE(Playback.Start(MustParse(Text), 0.0f, Error));

    // All 20 go out on the first frame, before a tick takes any: the tick loses input
    TickInput Tick;
    Playback.Update(TickSeconds);
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Provider.Build(Tick, TickWindow{});
    REQUIRE(Tick.Slice.Overran);
    REQUIRE(Playback.IsActive());

    // The next frame stops the replay and says why
    Playback.Update(TickSeconds);
    REQUIRE_FALSE(Playback.IsActive());
    REQUIRE(Playback.HasOverrun());
    REQUIRE(Playback.GetFinishedCount() == 0);
    REQUIRE_THAT(Playback.GetLog().GetLines().back(), Catch::Matchers::ContainsSubstring("Stopped"));
    bool Reported = false;
    for (const String& Line : Playback.GetLog().GetLines())
    {
        Reported = Reported || Line.find("Replay failed: a tick lost input") != String::npos;
    }
    REQUIRE(Reported);

    // A new replay starts with no overrun
    REQUIRE(Playback.Start(MustParse("{\"format\":\"fling-input\",\"version\":2,\"duration\":1.0}\n"), 0.0f, Error));
    REQUIRE_FALSE(Playback.HasOverrun());
}

TEST_CASE("InputPlayback waits for the start delay", "[Input][Playback]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputMessageProcessor Processor = Handler->CreateProcessor();
    InputPlayback Playback(Handler);

    String StartError;
    REQUIRE(
        Playback.Start(MustParse(String(Header) + "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"), 0.5f, StartError));
    CHECK(Playback.GetState() == InputPlaybackState::Countdown);
    CHECK(Handler->FindInterface(InputPlayback::InterfaceName) == nullptr);

    Playback.Update(0.25f);
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Processor.ConsumeUntil();
    CHECK(Playback.GetState() == InputPlaybackState::Countdown);
    CHECK_FALSE(IsHeldAnywhere(Processor, Keys::W));

    Playback.Update(0.25f);
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Processor.ConsumeUntil();
    CHECK(Playback.GetState() == InputPlaybackState::Playing);
    CHECK(IsHeldAnywhere(Processor, Keys::W));

    Playback.Stop();
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Processor.ConsumeUntil();
    CHECK(Playback.GetState() == InputPlaybackState::Idle);
    CHECK_FALSE(IsHeldAnywhere(Processor, Keys::W));
}

TEST_CASE("InputPlayback makes one device for each recorded device", "[Input][Playback]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputPlayback Playback(Handler);
    String StartError;
    REQUIRE(Playback.Start(MustParse(String(Header) + "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"
                                                      "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"MouseLeft\"}\n"
                                                      "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"GamepadFaceBottom\"}\n"),
                           0.0f,
                           StartError));

    const auto Devices = Handler->GetDeviceFactory().GetConnectedDevices();
    REQUIRE(Devices.size() == 3);
    CHECK(Devices[0]->GetId() != Devices[1]->GetId());
    CHECK(Devices[1]->GetId() != Devices[2]->GetId());
    CHECK(Devices[0]->GetId() != Devices[2]->GetId());
    CHECK(Devices[0]->GetInfo().Type == InputDeviceType::Keyboard);
    CHECK(Devices[0]->GetInfo().HardwarePath != Devices[1]->GetInfo().HardwarePath);
    CHECK_FALSE(Devices[0]->GetInfo().DisplayName.empty());
    Playback.Stop();
}

TEST_CASE("InputPlayback loads a file and reports a bad one", "[Input][Playback]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputPlayback Playback(Handler);
    String Error;

    CHECK_FALSE(Playback.Start(TempFile("missing.input.jsonl"), 0.0f, Error));
    CHECK_FALSE(Error.empty());
    CHECK(Playback.GetState() == InputPlaybackState::Idle);

    const String Path = TempFile("playback_file.input.jsonl");
    {
        std::ofstream File(Path);
        File << Header << "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n";
    }
    REQUIRE(Playback.Start(Path, 0.0f, Error));
    CHECK(Playback.GetState() == InputPlaybackState::Playing);
    CHECK(Playback.GetPath() == Path);
    CHECK(Playback.GetMessageCount() == 1);
    Playback.Stop();
}

TEST_CASE("Input Poll drives playback and recording", "[Input][Playback][Recorder]")
{
    struct Guard
    {
        Guard() { Input::Get().Init(); }

        ~Guard() { Input::Get().Shutdown(); }
    } InputGuard;

    Input& Singleton = Input::Get();
    const String Path = TempFile("singleton_record.input.jsonl");
    Singleton.GetRecorder().Start(Path, 0.0f, 0.0f);

    String StartError;
    REQUIRE(Singleton.GetPlayback().Start(MustParse("{\"format\":\"fling-input\",\"version\":1,\"duration\":0.2}\n"
                                                    "{\"t\":0.05,\"type\":\"KeyDown\",\"key\":\"W\"}\n"),
                                          0.0f,
                                          StartError));

    InputMessageProcessor Reader = Singleton.CreateProcessor();
    bool WasHeld = false;
    for (int Update = 0; Update < 60 && Singleton.GetPlayback().IsActive(); ++Update)
    {
        Singleton.Poll(TickSeconds);
        Reader.ConsumeUntil();
        WasHeld = WasHeld || IsHeldAnywhere(Reader, Keys::W);
    }
    CHECK(WasHeld);
    CHECK(Singleton.GetPlayback().GetFinishedCount() == 1);

    // The recorder saw the playback device's messages. Shutdown saves the file.
    CHECK(Singleton.GetRecorder().GetMessageCount() >= 1);
    Singleton.Shutdown();
    CHECK(std::filesystem::exists(Path));
    Singleton.Init();
}

TEST_CASE("A recording of a playback plays on the same ticks", "[Input][Playback][Recorder]")
{
    struct Guard
    {
        Guard() { Input::Get().Init(); }

        ~Guard() { Input::Get().Shutdown(); }
    } InputGuard;

    Input& Singleton = Input::Get();

    /** Poll at 60 ticks per second until playback ends, and keep the tick of each W press. */
    const auto PlayAndRecordPressTicks = [&](const InputRecording& t_Recording, const StringView t_RecordPath)
    {
        std::vector<int> PressTicks;
        if (!t_RecordPath.empty())
        {
            Singleton.GetRecorder().Start(t_RecordPath, 0.0f, 0.0f);
        }
        String StartError;
        TickInputBuilder Builder(Singleton.GetHandler());
        TickInput Taken;
        REQUIRE(Singleton.GetPlayback().Start(t_Recording, 0.0f, StartError));
        for (int Tick = 0; Tick < 600 && Singleton.GetPlayback().IsActive(); ++Tick)
        {
            Singleton.Poll(TickSeconds);
            Builder.Build(Taken);
            if (Taken.WasPressed(Keys::W))
            {
                PressTicks.push_back(Tick);
            }
        }
        if (!t_RecordPath.empty())
        {
            REQUIRE(Singleton.GetRecorder().Stop());
        }
        return PressTicks;
    };

    const InputRecording Original = MustParse("{\"format\":\"fling-input\",\"version\":1,\"duration\":5.0}\n"
                                              "{\"tick\":0,\"type\":\"Press\",\"key\":\"W\"}\n"
                                              "{\"tick\":1,\"type\":\"Press\",\"key\":\"W\"}\n"
                                              "{\"tick\":7,\"type\":\"Press\",\"key\":\"W\"}\n"
                                              "{\"tick\":59,\"type\":\"Press\",\"key\":\"W\"}\n"
                                              "{\"tick\":281,\"type\":\"Press\",\"key\":\"W\"}\n");

    const String Path = TempFile("rerecord.input.jsonl");
    const std::vector<int> FirstTicks = PlayAndRecordPressTicks(Original, Path);
    CHECK(FirstTicks == std::vector<int>{0, 1, 7, 59, 281});

    InputRecording Recorded;
    String Error;
    REQUIRE(LoadInputRecording(Path, Recorded, Error));
    const std::vector<int> SecondTicks = PlayAndRecordPressTicks(Recorded, "");
    CHECK(SecondTicks == FirstTicks);
}

TEST_CASE("InputRecorder can remove the click that stopped it", "[Input][Recorder]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    const std::shared_ptr<VirtualInputDeviceInterface> Virtual = std::make_shared<VirtualInputDeviceInterface>();
    Handler->AddInterface(Virtual);
    InputDeviceInfo KeyboardInfo;
    KeyboardInfo.Type = InputDeviceType::Keyboard;
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo);
    InputDeviceInfo MouseInfo;
    MouseInfo.Type = InputDeviceType::Mouse;
    MouseInfo.HardwarePath = "mouse";
    const InputDeviceId Mouse = Virtual->CreateDevice(MouseInfo);

    const auto Queue = [&](InputDeviceId t_Device, InputMessageType t_Type, InputKey t_Key)
    {
        InputMessage Message;
        Message.Device = t_Device;
        Message.Type = t_Type;
        Message.Key = t_Key;
        Virtual->Queue(Message);
    };

    InputRecorder Recorder(Handler);
    const String Path = TempFile("trim_click.input.jsonl");
    Recorder.Start(Path, 0.0f, 0.0f);
    Queue(Keyboard, InputMessageType::KeyDown, Keys::W);
    Queue(Mouse, InputMessageType::MouseMove, Keys::Invalid);
    Queue(Mouse, InputMessageType::KeyDown, Keys::MouseLeft);
    Queue(Mouse, InputMessageType::MouseMove, Keys::Invalid);
    Queue(Mouse, InputMessageType::KeyUp, Keys::MouseLeft);
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Recorder.Update(TickSeconds);
    // The start capture and the five messages
    REQUIRE(Recorder.GetMessageCount() == 6);

    REQUIRE(Recorder.Stop(true));
    InputRecording Saved;
    String Error;
    REQUIRE(LoadInputRecording(Path, Saved, Error));
    REQUIRE(Saved.Messages.size() == 3);
    CHECK(Saved.Messages[1].Message.Key == Keys::W);
    CHECK(Saved.Messages[2].Message.Type == InputMessageType::MouseMove);

    // A keyboard message after the click means the click did not stop the recording
    Recorder.Start(Path, 0.0f, 0.0f);
    Queue(Mouse, InputMessageType::KeyDown, Keys::MouseLeft);
    Queue(Keyboard, InputMessageType::KeyUp, Keys::W);
    Handler->PollInterfaces(PlatformTime::NowNanoseconds());
    Recorder.Update(TickSeconds);
    REQUIRE(Recorder.Stop(true));
    REQUIRE(LoadInputRecording(Path, Saved, Error));
    // W was held at the start, so it is written first as a KeyDown at time 0, then the start capture
    REQUIRE(Saved.Messages.size() == 4);
    CHECK(Saved.Messages[2].Message.Key == Keys::MouseLeft);
    CHECK(Saved.Messages[3].Message.Type == InputMessageType::KeyUp);
}
namespace
{
    /** What one run of the late poll loop gives: the mouse counts of each tick, and the samples pending on each frame.
     */
    struct LoopResult
    {
        std::vector<std::vector<int32>> TickCounts;
        std::vector<size_t> PendingPerFrame;
        /** The file that the recorder saved, if the run recorded. */
        String SavedPath;
    };

    /**
     * The engine loop with one tick per frame, as World::Update runs it: Update the
     * playback and poll, build the tick, then (if t_Late) UpdateLate and poll again, then
     * read what is pending. A recorder, if given, reads at the start of each frame (as
     * Input::Poll does).
     */
    LoopResult RunLateLoop(InputRecording t_Recording, const bool t_Late, const bool t_Record = false)
    {
        const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
        InputPlayback Playback(Handler);
        InputRecorder Recorder(Handler);
        TickInputBuilder Ticks(Handler);
        PendingInputReader Pending(Handler);
        String StartError;
        REQUIRE(Playback.Start(std::move(t_Recording), 0.0f, StartError));
        if (t_Record)
        {
            Recorder.Start(TempFile("late_poll.input.jsonl"), 0.0f, 0.0f);
        }
        LoopResult Result;
        for (int Frame = 0; Frame < 40; ++Frame)
        {
            Playback.Update(TickSeconds);
            Handler->PollInterfaces(PlatformTime::NowNanoseconds());
            if (t_Record)
            {
                Recorder.Update(TickSeconds);
            }

            TickInput Tick;
            Ticks.Build(Tick);
            std::vector<int32>& Counts = Result.TickCounts.emplace_back();
            for (const InputMouseSample& Sample : Tick.Slice.MouseSamples)
            {
                Counts.push_back(Sample.DeltaX);
            }

            if (t_Late)
            {
                Playback.UpdateLate();
                Handler->PollInterfaces(PlatformTime::NowNanoseconds());
            }
            InputSlice Data;
            Pending.Read(Tick.Slice.Cut.LastSequence, Data);
            Result.PendingPerFrame.push_back(Data.MouseSamples.size());
        }
        if (t_Record)
        {
            REQUIRE(Recorder.Stop());
            Result.SavedPath = Recorder.GetLastSavedPath();
        }
        return Result;
    }
} // namespace

TEST_CASE("InputPlayback::UpdateLate sends early what the next Update would send", "[Input][Playback]")
{
    // Moves at times between two frames and on them, two at one time, and a key
    String Text = "{\"format\":\"fling-input\",\"version\":1,\"tickRate\":120,\"duration\":0.5}\n";
    for (const int Tick : {3, 4, 7, 7, 10, 15, 16, 21, 30})
    {
        Text += "{\"tick\":" + std::to_string(Tick) + ",\"type\":\"MouseMove\",\"dx\":" + std::to_string(Tick) +
                ",\"dy\":0}\n";
    }
    Text += "{\"tick\":12,\"type\":\"Press\",\"key\":\"F\"}\n";
    const InputRecording Recording = MustParse(Text);

    const LoopResult Early = RunLateLoop(Recording, false);
    const LoopResult Late = RunLateLoop(Recording, true);

    // Every tick gets the same messages, in the same order, with or without the late poll
    REQUIRE(Late.TickCounts == Early.TickCounts);
    size_t Total = 0;
    for (const std::vector<int32>& Counts : Early.TickCounts)
    {
        Total += Counts.size();
    }
    REQUIRE(Total == 9);

    // Without it, a frame that ran a tick has nothing pending. With it, the frame before a
    // tick shows exactly what that tick will take.
    size_t PendingLate = 0;
    for (size_t Frame = 0; Frame < Early.PendingPerFrame.size(); ++Frame)
    {
        REQUIRE(Early.PendingPerFrame[Frame] == 0);
        if (Frame + 1 < Late.TickCounts.size())
        {
            REQUIRE(Late.PendingPerFrame[Frame] == Late.TickCounts[Frame + 1].size());
        }
        PendingLate += Late.PendingPerFrame[Frame];
    }
    UNSCOPED_INFO("pending samples over 40 frames: 0 without the late poll, " << PendingLate << " with it");
    REQUIRE(PendingLate == Total);

    SECTION("A recording of a run with the late poll plays on the same ticks")
    {
        const LoopResult Recorded = RunLateLoop(Recording, true, true);
        REQUIRE(Recorded.TickCounts == Early.TickCounts);

        InputRecording Saved;
        String Error;
        REQUIRE(LoadInputRecording(Recorded.SavedPath, Saved, Error));
        // The start capture, 9 moves, and the press and release of F
        REQUIRE(Saved.Messages.size() == 12);
        REQUIRE(RunLateLoop(Saved, true).TickCounts == Early.TickCounts);
        REQUIRE(RunLateLoop(Saved, false).TickCounts == Early.TickCounts);
    }
}
#endif // WITH_INPUT_RECORDING
