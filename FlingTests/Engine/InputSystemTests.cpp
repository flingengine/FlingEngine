#include "Components/InputSnapshot.h"
#include "Ecs/EntityStorage.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Input/InputMessageHandler.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "InputUtil.h"
#include "Systems/InputSystem.h"

#include <catch2/catch_all.hpp>
#include <memory>

using namespace Fling;

namespace
{
    InputDeviceInfo KeyboardInfo(const char* t_Path)
    {
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Keyboard;
        Info.HardwarePath = t_Path;
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

    /** A world that runs only Engine.Input, with a virtual keyboard on its own handler. */
    struct InputHarness
    {
        std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
        std::shared_ptr<VirtualInputDeviceInterface> Virtual;
        InputDeviceId Keyboard;

        EntityStorage Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;

        explicit InputHarness(const bool t_Connect = true)
        {
            const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
            Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);
            Keyboard = Virtual->CreateDevice(KeyboardInfo("virtual:kb"));

            Systems.Add<InputSystem>();
            ScheduleResult Schedule = Systems.BuildSchedule();
            REQUIRE(Schedule.Ok());
            Runner = std::make_unique<SystemRunner>(Systems, std::move(Schedule.Value), Storage);
            Runner->Create();
            if (t_Connect)
            {
                InputUtil::Connect(Storage, Handler);
            }
        }

        ~InputHarness() { Runner->Destroy(); }

        void Queue(InputKey t_Key, bool t_Down) { Queue(Keyboard, t_Key, t_Down); }

        void Queue(InputDeviceId t_Device, InputKey t_Key, bool t_Down)
        {
            Virtual->Queue(MakeKey(t_Device, t_Key, t_Down));
            Handler->PollInterfaces(0);
        }

        /** Run one fixed tick and give the snapshot that Engine.Input made. */
        const InputSnapshot& Tick()
        {
            Runner->RunTick(WorldRole::Play);
            return Storage.Singleton<InputSnapshot>();
        }
    };
} // namespace

TEST_CASE("Engine.Input builds one snapshot for each tick", "[Input][InputSystem]")
{
    InputHarness h;

    SECTION("A press is seen by exactly one tick")
    {
        h.Queue(Keys::T, true);

        const InputSnapshot& First = h.Tick();
        REQUIRE(First.WasPressed(Keys::T));
        REQUIRE(First.IsHeld(Keys::T));

        // A second tick with no new messages: still held, but the press is not seen again
        const InputSnapshot& Second = h.Tick();
        REQUIRE_FALSE(Second.WasPressed(Keys::T));
        REQUIRE(Second.IsHeld(Keys::T));
    }

    SECTION("A press and a release inside one tick are both counted")
    {
        h.Queue(Keys::A, true);
        h.Queue(Keys::A, false);

        const InputSnapshot& Snapshot = h.Tick();
        REQUIRE(Snapshot.WasPressed(Keys::A));
        REQUIRE(Snapshot.WasReleased(Keys::A));
        REQUIRE_FALSE(Snapshot.IsHeld(Keys::A));
    }

    SECTION("Two devices are merged")
    {
        const InputDeviceId Second = h.Virtual->CreateDevice(KeyboardInfo("virtual:kb2"));
        h.Queue(Keys::W, true);
        h.Queue(Second, Keys::W, true);

        const InputSnapshot& Snapshot = h.Tick();
        REQUIRE(Snapshot.GetKeyState(Keys::W).PressCount == 2);
        REQUIRE(Snapshot.Keys.size() == 1);
    }

    SECTION("A captured keyboard gives neutral input")
    {
        h.Queue(Keys::W, true);
        InputUtil::SetUICapture(h.Storage, true, false);

        const InputSnapshot& Snapshot = h.Tick();
        REQUIRE_FALSE(Snapshot.IsHeld(Keys::W));
        REQUIRE_FALSE(Snapshot.WasPressed(Keys::W));
    }

    SECTION("A handler that is destroyed gives empty input")
    {
        h.Queue(Keys::W, true);
        h.Virtual.reset();
        h.Handler.reset();

        REQUIRE(h.Tick().Keys.empty());
    }
}

TEST_CASE("Engine.Input in a world that is not connected gives empty input", "[Input][InputSystem]")
{
    InputHarness h(false);
    h.Queue(Keys::W, true);

    const InputSnapshot& Snapshot = h.Tick();
    REQUIRE(Snapshot.Keys.empty());
    REQUIRE(Snapshot.MouseDeltaX == 0);
}
