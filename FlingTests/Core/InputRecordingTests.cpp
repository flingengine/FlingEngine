#include <catch2/catch_all.hpp>

#include "Input/Input.h"
#include "Input/InputMessageHandler.h"
#include "Input/InputPlayback.h"
#include "Input/InputRecorder.h"
#include "Input/InputRecording.h"
#include "Input/VirtualInputDeviceInterface.h"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#if WITH_INPUT_RECORDING
namespace
{
    using namespace Fling;
    using Catch::Matchers::ContainsSubstring;
    using Catch::Matchers::StartsWith;

    constexpr float TickSeconds = 1.0f / 60.0f;

    /** A header line with no devices and a duration of 1 second. */
    const char* const Header = "{\"format\":\"fling-input\",\"version\":1,\"duration\":1.0}\n";

    InputRecording MustParse(const std::string& t_Text)
    {
        InputRecording Recording;
        std::string Error;
        INFO(t_Text);
        REQUIRE(ParseInputRecording(t_Text, Recording, Error));
        REQUIRE(Error.empty());
        return Recording;
    }

    std::string ParseError(const std::string& t_Text)
    {
        InputRecording Recording;
        std::string Error;
        REQUIRE_FALSE(ParseInputRecording(t_Text, Recording, Error));
        return Error;
    }

    std::string TempFile(const std::string& t_Name)
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
            Handler->PollInterfaces(InputNowNs());
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
    const std::string Text = "{\"format\":\"fling-input\",\"version\":1,\"duration\":4.0}\n"
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
            MustParse(std::string(Header) + "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"
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
        const std::string Error =
            ParseError(std::string(Header) + "{\"t\":0,\"device\":5,\"type\":\"KeyDown\",\"key\":\"W\"}\n");
        CHECK_THAT(Error, StartsWith("line 2:"));
        CHECK_THAT(Error, ContainsSubstring("5"));
    }

    SECTION("An unknown device type name is an error")
    {
        const std::string Error =
            ParseError(std::string(Header) + "{\"t\":0,\"device\":\"Toaster\",\"type\":\"KeyDown\",\"key\":\"W\"}\n");
        CHECK_THAT(Error, StartsWith("line 2:"));
    }
}

TEST_CASE("InputRecording expands Press", "[Input][Recording]")
{
    SECTION("With a duration")
    {
        const InputRecording Recording =
            MustParse(std::string(Header) + "{\"t\":1.0,\"type\":\"Press\",\"key\":\"Space\",\"duration\":0.25}\n");
        REQUIRE(Recording.Messages.size() == 2);
        CHECK(Recording.Messages[0].Message.Type == InputMessageType::KeyDown);
        CHECK(Recording.Messages[0].TimeSeconds == Catch::Approx(1.0));
        CHECK(Recording.Messages[1].Message.Type == InputMessageType::KeyUp);
        CHECK(Recording.Messages[1].TimeSeconds == Catch::Approx(1.25));
    }

    SECTION("With no duration, both happen at the same time")
    {
        const InputRecording Recording =
            MustParse(std::string(Header) + "{\"t\":1.0,\"type\":\"Press\",\"key\":\"Space\"}\n");
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
        MustParse(std::string(Header) + "{\"t\":0.5,\"type\":\"Char\",\"text\":\"h\xC3\xA9y\"}\n");
    REQUIRE(Recording.Messages.size() == 3);
    CHECK(Recording.Messages[0].Message.CodePoint == 0x68);
    CHECK(Recording.Messages[1].Message.CodePoint == 0xE9);
    CHECK(Recording.Messages[2].Message.CodePoint == 0x79);
    CHECK(Recording.Messages[2].Message.Type == InputMessageType::Char);
}

TEST_CASE("InputRecording gives mouse moves a position", "[Input][Recording]")
{
    const InputRecording Recording =
        MustParse(std::string(Header) + "{\"t\":0.1,\"type\":\"MouseMove\",\"dx\":3,\"dy\":4}\n"
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
        CHECK_THAT(ParseError("{\"format\":\"fling-input\",\"version\":2}\n"), StartsWith("line 1:"));
        CHECK_THAT(ParseError("not json\n"), StartsWith("line 1:"));
    }

    SECTION("An unknown key")
    {
        const std::string Error =
            ParseError(std::string(Header) + "# comment\n{\"t\":0,\"type\":\"KeyDown\",\"key\":\"NoSuchKey\"}\n");
        CHECK_THAT(Error, StartsWith("line 3:"));
        CHECK_THAT(Error, ContainsSubstring("NoSuchKey"));
    }

    SECTION("An unknown message type")
    {
        CHECK_THAT(ParseError(std::string(Header) + "{\"t\":0,\"type\":\"Explode\"}\n"), StartsWith("line 2:"));
    }

    SECTION("Both t and tick")
    {
        CHECK_THAT(ParseError(std::string(Header) + "\n{\"t\":0,\"tick\":1,\"type\":\"KeyDown\",\"key\":\"W\"}\n"),
                   StartsWith("line 3:"));
    }

    SECTION("Neither t nor tick")
    {
        CHECK_THAT(ParseError(std::string(Header) + "{\"type\":\"KeyDown\",\"key\":\"W\"}\n"), StartsWith("line 2:"));
    }

    SECTION("A key that is not an axis in an Axis message")
    {
        CHECK_THAT(ParseError(std::string(Header) + "{\"t\":0,\"type\":\"Axis\",\"key\":\"W\",\"value\":1}\n"),
                   StartsWith("line 2:"));
    }

    SECTION("An Axis message with no value")
    {
        CHECK_THAT(ParseError(std::string(Header) + "{\"t\":0,\"type\":\"Axis\",\"key\":\"GamepadLeftX\"}\n"),
                   StartsWith("line 2:"));
    }

    SECTION("A message line that is not valid JSON")
    {
        CHECK_THAT(ParseError(std::string(Header) + "{\"t\":0,\n"), StartsWith("line 2:"));
    }

    SECTION("A file that is missing")
    {
        InputRecording Recording;
        std::string Error;
        CHECK_FALSE(LoadInputRecording(TempFile("does_not_exist.input.jsonl"), Recording, Error));
        CHECK_FALSE(Error.empty());
    }
}

TEST_CASE("InputRecording sorts messages by time", "[Input][Recording]")
{
    const InputRecording Recording = MustParse(std::string(Header) + "{\"t\":2,\"type\":\"KeyUp\",\"key\":\"W\"}\n"
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

    const std::string Text = WriteInputRecording(Recording);
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

TEST_CASE("InputRecording saves and loads a file", "[Input][Recording]")
{
    const std::string Path = TempFile("nested/folder/save_load.input.jsonl");
    InputRecording Recording = MustParse(std::string(Header) + "{\"t\":0.5,\"type\":\"KeyDown\",\"key\":\"W\"}\n");
    std::string Error;
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
    const std::string Path = TempFile("recorder_basic.input.jsonl");

    Recorder.Start(Path, 0.0f, 0.0f);
    REQUIRE(Recorder.GetState() == InputRecorderState::Recording);

    Fixture.Send(Keys::W, true);
    Recorder.Update(0.1f);
    Fixture.Send(Keys::W, false);
    Recorder.Update(0.1f);
    CHECK(Recorder.GetMessageCount() == 2);

    REQUIRE(Recorder.Stop());
    CHECK(Recorder.GetState() == InputRecorderState::Idle);
    CHECK(Recorder.GetSaveCount() == 1);
    CHECK(Recorder.GetLastSavedPath() == Path);
    CHECK(Recorder.GetLastError().empty());
    CHECK_FALSE(Recorder.GetLog().GetLines().empty());

    InputRecording Loaded;
    std::string Error;
    REQUIRE(LoadInputRecording(Path, Loaded, Error));
    REQUIRE(Loaded.Messages.size() == 2);
    CHECK(Loaded.Messages[0].Message.Type == InputMessageType::KeyDown);
    CHECK(Loaded.Messages[0].Message.Key == Keys::W);
    CHECK(Loaded.Messages[1].Message.Type == InputMessageType::KeyUp);
    CHECK(Loaded.Messages[1].TimeSeconds >= Loaded.Messages[0].TimeSeconds);
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
    CHECK(Recorder.GetMessageCount() == 0);
    REQUIRE(Recorder.Stop());
}

TEST_CASE("InputRecorder writes held keys at time 0", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    Fixture.Send(Keys::W, true);

    InputRecorder Recorder(Fixture.Handler);
    const std::string Path = TempFile("recorder_held.input.jsonl");
    Recorder.Start(Path, 0.0f, 0.0f);
    REQUIRE(Recorder.Stop());

    InputRecording Loaded;
    std::string Error;
    REQUIRE(LoadInputRecording(Path, Loaded, Error));
    REQUIRE(Loaded.Messages.size() == 1);
    CHECK(Loaded.Messages[0].Message.Type == InputMessageType::KeyDown);
    CHECK(Loaded.Messages[0].Message.Key == Keys::W);
    CHECK(Loaded.Messages[0].TimeSeconds == 0.0);
}

TEST_CASE("InputRecorder counts down before it records", "[Input][Recorder]")
{
    RecorderFixture Fixture;
    InputRecorder Recorder(Fixture.Handler);
    const std::string Path = TempFile("recorder_countdown.input.jsonl");

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
        const std::string OtherPath = TempFile("recorder_cancelled.input.jsonl");
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
    const std::string Path = TempFile("recorder_duration.input.jsonl");

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
    const std::string Path = std::filesystem::temp_directory_path().string();
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

    Playback.Start(MustParse("{\"format\":\"fling-input\",\"version\":1,\"duration\":1.0}\n"
                             "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"A\"}\n"
                             "{\"t\":0.5,\"type\":\"KeyDown\",\"key\":\"W\"}\n"),
                   0.0f);
    REQUIRE(Playback.GetState() == InputPlaybackState::Playing);
    CHECK(Handler->FindInterface(InputPlayback::InterfaceName) != nullptr);

    int FinishedAt = -1;
    for (int Update = 0; Update < 100; ++Update)
    {
        Playback.Update(TickSeconds);
        Handler->PollInterfaces(InputNowNs());
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

TEST_CASE("InputPlayback waits for the start delay", "[Input][Playback]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputMessageProcessor Processor = Handler->CreateProcessor();
    InputPlayback Playback(Handler);

    Playback.Start(MustParse(std::string(Header) + "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"), 0.5f);
    CHECK(Playback.GetState() == InputPlaybackState::Countdown);
    CHECK(Handler->FindInterface(InputPlayback::InterfaceName) == nullptr);

    Playback.Update(0.25f);
    Handler->PollInterfaces(InputNowNs());
    Processor.ConsumeUntil();
    CHECK(Playback.GetState() == InputPlaybackState::Countdown);
    CHECK_FALSE(IsHeldAnywhere(Processor, Keys::W));

    Playback.Update(0.25f);
    Handler->PollInterfaces(InputNowNs());
    Processor.ConsumeUntil();
    CHECK(Playback.GetState() == InputPlaybackState::Playing);
    CHECK(IsHeldAnywhere(Processor, Keys::W));

    Playback.Stop();
    Handler->PollInterfaces(InputNowNs());
    Processor.ConsumeUntil();
    CHECK(Playback.GetState() == InputPlaybackState::Idle);
    CHECK_FALSE(IsHeldAnywhere(Processor, Keys::W));
}

TEST_CASE("InputPlayback makes one device for each recorded device", "[Input][Playback]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    InputPlayback Playback(Handler);
    Playback.Start(MustParse(std::string(Header) + "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"W\"}\n"
                                                   "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"MouseLeft\"}\n"
                                                   "{\"t\":0,\"type\":\"KeyDown\",\"key\":\"GamepadFaceBottom\"}\n"),
                   0.0f);

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
    std::string Error;

    CHECK_FALSE(Playback.Start(TempFile("missing.input.jsonl"), 0.0f, Error));
    CHECK_FALSE(Error.empty());
    CHECK(Playback.GetState() == InputPlaybackState::Idle);

    const std::string Path = TempFile("playback_file.input.jsonl");
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
    const std::string Path = TempFile("singleton_record.input.jsonl");
    Singleton.GetRecorder().Start(Path, 0.0f, 0.0f);

    Singleton.GetPlayback().Start(MustParse("{\"format\":\"fling-input\",\"version\":1,\"duration\":0.2}\n"
                                            "{\"t\":0.05,\"type\":\"KeyDown\",\"key\":\"W\"}\n"),
                                  0.0f);

    bool WasHeld = false;
    for (int Update = 0; Update < 60 && Singleton.GetPlayback().IsActive(); ++Update)
    {
        Singleton.Poll(TickSeconds);
        Singleton.BeginTick(InputNowNs());
        WasHeld = WasHeld || Singleton.IsHeld(Keys::W);
        Singleton.EndTick();
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
    const auto PlayAndRecordPressTicks = [&](const InputRecording& t_Recording, const std::string& t_RecordPath)
    {
        std::vector<int> PressTicks;
        if (!t_RecordPath.empty())
        {
            Singleton.GetRecorder().Start(t_RecordPath, 0.0f, 0.0f);
        }
        Singleton.GetPlayback().Start(t_Recording, 0.0f);
        for (int Tick = 0; Tick < 600 && Singleton.GetPlayback().IsActive(); ++Tick)
        {
            Singleton.Poll(TickSeconds);
            Singleton.BeginTick();
            if (Singleton.WasPressed(Keys::W))
            {
                PressTicks.push_back(Tick);
            }
            Singleton.EndTick();
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

    const std::string Path = TempFile("rerecord.input.jsonl");
    const std::vector<int> FirstTicks = PlayAndRecordPressTicks(Original, Path);
    CHECK(FirstTicks == std::vector<int>{0, 1, 7, 59, 281});

    InputRecording Recorded;
    std::string Error;
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
    const std::string Path = TempFile("trim_click.input.jsonl");
    Recorder.Start(Path, 0.0f, 0.0f);
    Queue(Keyboard, InputMessageType::KeyDown, Keys::W);
    Queue(Mouse, InputMessageType::MouseMove, Keys::Invalid);
    Queue(Mouse, InputMessageType::KeyDown, Keys::MouseLeft);
    Queue(Mouse, InputMessageType::MouseMove, Keys::Invalid);
    Queue(Mouse, InputMessageType::KeyUp, Keys::MouseLeft);
    Handler->PollInterfaces(InputNowNs());
    Recorder.Update(TickSeconds);
    REQUIRE(Recorder.GetMessageCount() == 5);

    REQUIRE(Recorder.Stop(true));
    InputRecording Saved;
    std::string Error;
    REQUIRE(LoadInputRecording(Path, Saved, Error));
    REQUIRE(Saved.Messages.size() == 2);
    CHECK(Saved.Messages[0].Message.Key == Keys::W);
    CHECK(Saved.Messages[1].Message.Type == InputMessageType::MouseMove);

    // A keyboard message after the click means the click did not stop the recording
    Recorder.Start(Path, 0.0f, 0.0f);
    Queue(Mouse, InputMessageType::KeyDown, Keys::MouseLeft);
    Queue(Keyboard, InputMessageType::KeyUp, Keys::W);
    Handler->PollInterfaces(InputNowNs());
    Recorder.Update(TickSeconds);
    REQUIRE(Recorder.Stop(true));
    REQUIRE(LoadInputRecording(Path, Saved, Error));
    // W was held at the start, so it is written first as a KeyDown at time 0
    REQUIRE(Saved.Messages.size() == 3);
    CHECK(Saved.Messages[1].Message.Key == Keys::MouseLeft);
    CHECK(Saved.Messages[2].Message.Type == InputMessageType::KeyUp);
}
#endif // WITH_INPUT_RECORDING
