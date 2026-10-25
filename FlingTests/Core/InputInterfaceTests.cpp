#include <catch2/catch_all.hpp>

#include "Input/InputMessageHandler.h"
#include "Input/VirtualInputDeviceInterface.h"
#include <functional>
#include <memory>
#include <string>

namespace
{
    using namespace Fling;

    /** Counts every call, and can run code from inside Poll. */
    class TestInterface : public IInputDeviceInterface
    {
    public:
        const char* GetName() const override { return "Test"; }

        void Init(const std::shared_ptr<InputMessageHandler>& t_Handler) override
        {
            ++InitCount;
            Handler = t_Handler;
        }

        void Shutdown() override { ++ShutdownCount; }

        bool WantsPoll(int64) const override { return PollWanted; }

        void Poll(int64) override
        {
            ++PollCount;
            // Call a copy, so the callback can clear OnPoll while it runs.
            if (std::function<void()> Callback = OnPoll)
            {
                Callback();
            }
        }

        void OnWindowCreated(WindowHandle t_Window) override
        {
            ++WindowCreatedCount;
            LastWindow = t_Window;
        }

        void OnWindowDestroyed(WindowHandle) override { ++WindowDestroyedCount; }

        InputDeviceId MakeDevice(const char* t_Path)
        {
            InputDeviceInfo Info;
            Info.Type = InputDeviceType::Gamepad;
            Info.HardwarePath = t_Path;
            return Handler.lock()->GetDeviceFactory().CreateDevice(Info, *this)->GetId();
        }

        int InitCount = 0;
        int ShutdownCount = 0;
        int PollCount = 0;
        int WindowCreatedCount = 0;
        int WindowDestroyedCount = 0;
        bool PollWanted = true;
        WindowHandle LastWindow;
        std::function<void()> OnPoll;
        std::weak_ptr<InputMessageHandler> Handler;
    };

    /** Reports its destruction through a flag. */
    class DestroyFlagInterface : public TestInterface
    {
    public:
        explicit DestroyFlagInterface(bool* t_Flag) : m_Flag(t_Flag) {}

        ~DestroyFlagInterface() override { *m_Flag = true; }

    private:
        bool* m_Flag;
    };

    InputDeviceInfo KeyboardInfo()
    {
        InputDeviceInfo Info;
        Info.Type = InputDeviceType::Virtual;
        Info.HardwarePath = "virtual:kb";
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
} // namespace

TEST_CASE("Input interfaces at runtime", "[Input]")
{
    auto Handler = InputMessageHandler::Create();

    SECTION("Add calls Init and remove calls Shutdown")
    {
        auto Test = std::make_shared<TestInterface>();
        const InputInterfaceId Id = Handler->AddInterface(Test);
        REQUIRE(Id.IsValid());
        REQUIRE(Test->GetInterfaceId() == Id);
        REQUIRE(Test->InitCount == 1);
        REQUIRE(Handler->FindInterface(Id) == Test);

        REQUIRE(Handler->FindInterface("Test") == Test);
        REQUIRE(Handler->GetInterfaces().size() == 1);

        bool Destroyed = false;
        const InputInterfaceId FlaggedId = Handler->AddInterface<DestroyFlagInterface>(&Destroyed);
        REQUIRE(Handler->RemoveInterface(FlaggedId));
        REQUIRE(Destroyed);
        REQUIRE(Handler->FindInterface(FlaggedId) == nullptr);
        REQUIRE(Handler->GetInterfaces().size() == 1);
    }

    SECTION("A null interface and an unknown ID are rejected")
    {
        REQUIRE_FALSE(Handler->AddInterface(std::unique_ptr<IInputDeviceInterface>()).IsValid());
        REQUIRE_FALSE(Handler->RemoveInterface(InputInterfaceId{99}));
    }

    SECTION("Interfaces are listed in the order they were added")
    {
        auto A = std::make_shared<TestInterface>();
        auto B = std::make_shared<TestInterface>();
        Handler->AddInterface(A);
        Handler->AddInterface(B);
        const auto List = Handler->GetInterfaces();
        REQUIRE(List.size() == 2);
        REQUIRE(List[0] == A);
        REQUIRE(List[1] == B);
    }

    SECTION("Removing an interface disconnects its devices")
    {
        auto Test = std::make_shared<TestInterface>();
        const InputInterfaceId Id = Handler->AddInterface(Test);
        const InputDeviceId Device = Test->MakeDevice("pad0");
        REQUIRE(Handler->GetDeviceFactory().Find(Device)->IsConnected());

        Handler->RemoveInterface(Id);
        REQUIRE_FALSE(Handler->GetDeviceFactory().Find(Device)->IsConnected());
        REQUIRE(Handler->GetDeviceFactory().GetConnectedDevices().empty());
    }

    SECTION("The handler destructor shuts down interfaces")
    {
        bool Destroyed = false;
        {
            auto Local = InputMessageHandler::Create();
            Local->AddInterface(std::make_shared<DestroyFlagInterface>(&Destroyed));
        }
        REQUIRE(Destroyed);
    }

    SECTION("An interface added after the window gets OnWindowCreated")
    {
        WindowHandle Window;
        Window.Native = &Window;
        Handler->NotifyWindowCreated(Window);

        auto Test = std::make_shared<TestInterface>();
        Handler->AddInterface(Test);
        REQUIRE(Test->WindowCreatedCount == 1);
        REQUIRE(Test->LastWindow == Window);
        REQUIRE(Handler->GetCurrentWindow() == Window);

        Handler->NotifyWindowDestroyed(Window);
        REQUIRE(Test->WindowDestroyedCount == 1);
        REQUIRE_FALSE(Handler->GetCurrentWindow().IsValid());
    }

    SECTION("WantsPoll false skips Poll")
    {
        auto Test = std::make_shared<TestInterface>();
        Handler->AddInterface(Test);
        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Test->PollCount == 1);

        Test->PollWanted = false;
        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Test->PollCount == 1);
    }

    SECTION("An add requested inside Poll waits until the end of PollInterfaces")
    {
        auto Adder = std::make_shared<TestInterface>();
        auto Late = std::make_shared<TestInterface>();
        InputInterfaceId LateId;
        Adder->OnPoll = [&]()
        {
            Adder->OnPoll = nullptr;
            LateId = Handler->AddInterface(Late);
            REQUIRE(Late->InitCount == 0);
            REQUIRE(Handler->GetInterfaces().size() == 1);
            REQUIRE(Handler->FindInterface(LateId) == Late);
        };
        Handler->AddInterface(Adder);

        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Late->InitCount == 1);
        REQUIRE(Late->PollCount == 0);
        REQUIRE(Handler->GetInterfaces().size() == 2);

        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Late->PollCount == 1);
    }

    SECTION("A remove requested inside Poll waits until the end of PollInterfaces")
    {
        auto Remover = std::make_shared<TestInterface>();
        bool Destroyed = false;
        InputInterfaceId VictimId;
        Remover->OnPoll = [&]()
        {
            Remover->OnPoll = nullptr;
            REQUIRE(Handler->RemoveInterface(VictimId));
            REQUIRE_FALSE(Destroyed);
        };
        Handler->AddInterface(Remover);
        VictimId = Handler->AddInterface<DestroyFlagInterface>(&Destroyed);

        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Destroyed);
        REQUIRE(Handler->GetInterfaces().size() == 1);
    }

    SECTION("An interface can remove itself inside Poll")
    {
        bool Destroyed = false;
        const InputInterfaceId Id = Handler->AddInterface<DestroyFlagInterface>(&Destroyed);
        // Only the handler owns the interface, so use a plain pointer to set the callback.
        auto* Self = static_cast<DestroyFlagInterface*>(Handler->FindInterface(Id).get());
        Self->OnPoll = [&]()
        {
            Handler->RemoveInterface(Id);
        };

        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Destroyed);
        REQUIRE(Handler->GetInterfaces().empty());
    }
}

TEST_CASE("Input interface kept by the caller after removal", "[Input]")
{
    auto Handler = InputMessageHandler::Create();
    bool Destroyed = false;
    auto Kept = std::make_shared<DestroyFlagInterface>(&Destroyed);
    std::weak_ptr<DestroyFlagInterface> Weak = Kept;
    const InputInterfaceId Id = Handler->AddInterface(Kept);

    REQUIRE(Handler->RemoveInterface(Id));
    REQUIRE(Kept->ShutdownCount == 1);
    REQUIRE(Handler->GetInterfaces().empty());
    REQUIRE(Handler->FindInterface(Id) == nullptr);
    REQUIRE_FALSE(Destroyed);
    REQUIRE_FALSE(Weak.expired());

    Kept.reset();
    REQUIRE(Destroyed);
    REQUIRE(Weak.expired());
}

TEST_CASE("Input interface removes another one inside OnWindowCreated", "[Input]")
{
    class RemovingInterface : public TestInterface
    {
    public:
        void OnWindowCreated(WindowHandle t_Window) override
        {
            TestInterface::OnWindowCreated(t_Window);
            if (auto Locked = Handler.lock())
            {
                RemoveResult = Locked->RemoveInterface(VictimId);
            }
        }

        InputInterfaceId VictimId;
        bool RemoveResult = false;
    };

    auto Handler = InputMessageHandler::Create();
    auto Remover = std::make_shared<RemovingInterface>();
    auto Victim = std::make_shared<TestInterface>();
    Handler->AddInterface(Remover);
    Remover->VictimId = Handler->AddInterface(Victim);

    WindowHandle Window;
    Window.Native = &Window;
    Handler->NotifyWindowCreated(Window);

    REQUIRE(Remover->RemoveResult);
    REQUIRE(Remover->WindowCreatedCount == 1);
    REQUIRE(Victim->ShutdownCount == 1);
    REQUIRE(Handler->GetInterfaces().size() == 1);
    REQUIRE(Handler->FindInterface(Remover->VictimId) == nullptr);
}

TEST_CASE("Virtual input interface", "[Input]")
{
    auto Handler = InputMessageHandler::Create();

    auto Virtual = std::make_shared<VirtualInputDeviceInterface>();
    Handler->AddInterface(Virtual);
    const InputDeviceId Keyboard = Virtual->CreateDevice(KeyboardInfo());

    SECTION("A new interface with the same device info gets the same ID")
    {
        Handler->RemoveInterface(Virtual->GetInterfaceId());
        REQUIRE(Handler->GetDeviceFactory().GetConnectedDevices().empty());

        auto Second = std::make_shared<VirtualInputDeviceInterface>();
        Handler->AddInterface(Second);
        REQUIRE(Second->CreateDevice(KeyboardInfo()) == Keyboard);
    }

    SECTION("Messages arrive only on Poll")
    {
        InputMessageProcessor Processor = Handler->CreateProcessor();
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        REQUIRE(Virtual->GetQueuedCount() == 1);

        Processor.ConsumeUntil();
        REQUIRE_FALSE(Processor.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);

        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Virtual->GetQueuedCount() == 0);
        Processor.ConsumeUntil();
        REQUIRE(Processor.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);
    }

    SECTION("Messages for a foreign device are dropped")
    {
        Virtual->Queue(MakeKey(InputDeviceId{999}, Keys::W, true));
        REQUIRE(Virtual->GetQueuedCount() == 0);

        const uint64 Before = Handler->GetLatestSequence();
        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Handler->GetLatestSequence() == Before);
    }

    SECTION("Messages for a disconnected device are dropped")
    {
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Virtual->DisconnectDevice(Keyboard);

        const uint64 Before = Handler->GetLatestSequence();
        Handler->PollInterfaces(InputNowNs());
        REQUIRE(Handler->GetLatestSequence() == Before);
    }
}

TEST_CASE("Devices and messages of a virtual interface are simulated", "[Input][Interface]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    const std::shared_ptr<TestInterface> Hardware = std::make_shared<TestInterface>();
    const std::shared_ptr<VirtualInputDeviceInterface> Virtual = std::make_shared<VirtualInputDeviceInterface>();
    Handler->AddInterface(Hardware);
    Handler->AddInterface(Virtual);
    CHECK_FALSE(Hardware->IsSimulated());
    CHECK(Virtual->IsSimulated());

    const InputDeviceId HardwareId = Hardware->MakeDevice("pad0");
    InputDeviceInfo Info;
    Info.Type = InputDeviceType::Gamepad;
    Info.HardwarePath = "pad0";
    const InputDeviceId VirtualId = Virtual->CreateDevice(Info);
    CHECK(HardwareId != VirtualId);
    CHECK_FALSE(Handler->GetDeviceFactory().Find(HardwareId)->IsSimulated());
    CHECK(Handler->GetDeviceFactory().Find(VirtualId)->IsSimulated());

    InputMessageProcessor Processor = Handler->CreateProcessor();

    // The handler sets the flag from the device. A message cannot claim to be hardware
    InputMessage FromVirtual;
    FromVirtual.Device = VirtualId;
    FromVirtual.Type = InputMessageType::KeyDown;
    FromVirtual.Key = Keys::GamepadFaceBottom;
    FromVirtual.Simulated = false;
    Virtual->Queue(FromVirtual);
    Handler->PollInterfaces(InputNowNs());

    InputMessage FromHardware = FromVirtual;
    FromHardware.Device = HardwareId;
    FromHardware.Simulated = true;
    Handler->HandleMessage(FromHardware);

    InputMessage Read[2];
    REQUIRE(Processor.Read(Read, 2) == 2);
    CHECK(Read[0].Device == VirtualId);
    CHECK(Read[0].Simulated);
    CHECK(Read[1].Device == HardwareId);
    CHECK_FALSE(Read[1].Simulated);
    CHECK(DescribeInputMessage(Read[0], "Pad", 0).find("[simulated]") != std::string::npos);
    CHECK(DescribeInputMessage(Read[1], "Pad", 0).find("[simulated]") == std::string::npos);
}

TEST_CASE("Virtual devices have (Simulated) at the end of their name", "[Input][Interface]")
{
    const std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
    const std::shared_ptr<VirtualInputDeviceInterface> Virtual = std::make_shared<VirtualInputDeviceInterface>();
    Handler->AddInterface(Virtual);

    InputDeviceInfo Info;
    Info.Type = InputDeviceType::Gamepad;
    Info.DisplayName = "Xbox Controller";
    const InputDeviceId Pad = Virtual->CreateDevice(Info);
    CHECK(Handler->GetDeviceFactory().Find(Pad)->GetInfo().DisplayName == "Xbox Controller (Simulated)");

    // A name that has the suffix already does not get it again
    Info.DisplayName = "Keyboard (Simulated)";
    Info.Type = InputDeviceType::Keyboard;
    const InputDeviceId Keyboard = Virtual->CreateDevice(Info);
    CHECK(Handler->GetDeviceFactory().Find(Keyboard)->GetInfo().DisplayName == "Keyboard (Simulated)");
}
