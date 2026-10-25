#include <catch2/catch_all.hpp>

#include "Input/InputMessageHandler.h"
#include "Input/VirtualInputDeviceInterface.h"
#include <memory>
#include <vector>

namespace
{
    using namespace Fling;

    InputDeviceInfo MakeInfo(InputDeviceType t_Type, const char* t_Path)
    {
        InputDeviceInfo Info;
        Info.Type = t_Type;
        Info.DisplayName = t_Path;
        Info.HardwarePath = t_Path;
        Info.Capabilities = InputDeviceCapabilities::Keys;
        return Info;
    }

    /** An interface with a name that the test chooses. It has no devices of its own. */
    class NamedInterface : public IInputDeviceInterface
    {
    public:
        explicit NamedInterface(const char* t_Name) : m_Name(t_Name) {}

        const char* GetName() const override { return m_Name; }

        void Init(const std::shared_ptr<InputMessageHandler>& t_Handler) override { m_Handler = t_Handler; }

        void Shutdown() override {}

        void Poll(int64) override {}

        InputDeviceId Create(const InputDeviceInfo& t_Info)
        {
            std::shared_ptr<InputMessageHandler> Handler = m_Handler.lock();
            return Handler->GetDeviceFactory().CreateDevice(t_Info, *this)->GetId();
        }

    private:
        const char* m_Name;
        std::weak_ptr<InputMessageHandler> m_Handler;
    };

    struct Listener
    {
        std::vector<uint32> Connected;
        std::vector<uint32> Disconnected;
        std::vector<uint32> Order;

        void OnConnected(const InputDevice& t_Device)
        {
            Connected.push_back(t_Device.GetId().Value);
            Order.push_back(t_Device.GetId().Value);
        }

        void OnDisconnected(const InputDevice& t_Device)
        {
            Disconnected.push_back(t_Device.GetId().Value);
            Order.push_back(t_Device.GetId().Value);
        }
    };

    InputMessage MakeKey(InputDeviceId t_Device, InputKey t_Key, bool t_Down, int64 t_TimeNs = 0)
    {
        InputMessage Message;
        Message.Device = t_Device;
        Message.Type = t_Down ? InputMessageType::KeyDown : InputMessageType::KeyUp;
        Message.Key = t_Key;
        Message.TimeNs = t_TimeNs;
        return Message;
    }
} // namespace

TEST_CASE("Input handler sequence and time", "[Input]")
{
    auto Handler = InputMessageHandler::Create();
    const InputDeviceId Device{1};

    SECTION("Sequence strictly increases")
    {
        Handler->HandleMessage(MakeKey(Device, Keys::W, true, 100));
        Handler->HandleMessage(MakeKey(Device, Keys::W, false, 200));
        Handler->HandleMessage(MakeKey(Device, Keys::A, true, 300));
        REQUIRE(Handler->GetLatestSequence() == 3);

        InputMessage Message;
        uint64 Previous = 0;
        for (uint64 Sequence = 1; Sequence <= 3; ++Sequence)
        {
            REQUIRE(Handler->GetQueue().TryGet(Sequence, Message));
            REQUIRE(Message.Sequence > Previous);
            Previous = Message.Sequence;
        }
    }

    SECTION("An earlier time is clamped up")
    {
        Handler->HandleMessage(MakeKey(Device, Keys::W, true, 1000));
        Handler->HandleMessage(MakeKey(Device, Keys::W, false, 500));

        InputMessage Message;
        REQUIRE(Handler->GetQueue().TryGet(2, Message));
        REQUIRE(Message.TimeNs == 1000);
    }

    SECTION("A time of 0 becomes now")
    {
        const int64 Before = InputNowNs();
        Handler->HandleMessage(MakeKey(Device, Keys::W, true, 0));
        const int64 After = InputNowNs();

        InputMessage Message;
        REQUIRE(Handler->GetQueue().TryGet(1, Message));
        REQUIRE(Message.TimeNs >= Before);
        REQUIRE(Message.TimeNs <= After);
    }
}

TEST_CASE("Input device factory", "[Input]")
{
    auto Handler = InputMessageHandler::Create();
    InputDeviceFactory& Factory = Handler->GetDeviceFactory();

    auto Virtual = std::make_shared<VirtualInputDeviceInterface>();
    Handler->AddInterface(Virtual);

    SECTION("Devices get unique IDs")
    {
        const InputDeviceId A = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        const InputDeviceId B = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "b"));
        REQUIRE(A.IsValid());
        REQUIRE(B.IsValid());
        REQUIRE(A != B);
    }

    SECTION("The same identity gives the same device while it is connected")
    {
        const InputDeviceId A = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        const InputDeviceId Again = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        REQUIRE(A == Again);
        REQUIRE(Factory.GetDevices().size() == 1);
    }

    SECTION("Connecting again reuses the ID")
    {
        const InputDeviceId A = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        Virtual->DisconnectDevice(A);
        REQUIRE_FALSE(Factory.Find(A)->IsConnected());
        REQUIRE_FALSE(Factory.Find(A)->GetOwner().IsValid());

        const InputDeviceId Again = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        REQUIRE(Again == A);
        REQUIRE(Factory.Find(A)->IsConnected());
        REQUIRE(Factory.Find(A)->GetOwner() == Virtual->GetInterfaceId());
    }

    SECTION("A different hardware path gives a new ID")
    {
        const InputDeviceId A = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        Virtual->DisconnectDevice(A);
        const InputDeviceId B = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "b"));
        REQUIRE(A != B);
    }

    SECTION("The same identity from an interface with another name gives a new ID")
    {
        const InputDeviceId A = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));

        auto Other = std::make_shared<NamedInterface>("Other");
        Handler->AddInterface(Other);
        const InputDeviceId B = Other->Create(MakeInfo(InputDeviceType::Virtual, "a"));
        REQUIRE(A != B);
    }

    SECTION("Disconnect releases held keys first")
    {
        const InputDeviceId Keyboard = Virtual->CreateDevice(MakeInfo(InputDeviceType::Keyboard, "kb"));
        Virtual->Queue(MakeKey(Keyboard, Keys::W, true));
        Handler->PollInterfaces(InputNowNs());

        InputMessageProcessor Processor = Handler->CreateProcessor();
        REQUIRE(Processor.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);

        Virtual->DisconnectDevice(Keyboard);

        InputMessage Out[8];
        const uint32 Count = Processor.Read(Out, 8);
        REQUIRE(Count == 2);
        REQUIRE(Out[0].Type == InputMessageType::KeyUp);
        REQUIRE(Out[0].Key == Keys::W);
        REQUIRE(Out[1].Type == InputMessageType::DeviceDisconnected);

        Processor.ConsumeUntil();
        REQUIRE_FALSE(Processor.GetState().GetKeyState(Keyboard, Keys::W).IsHeld);
        REQUIRE_FALSE(Processor.GetState().Find(Keyboard)->Connected);
    }

    SECTION("Deferred events fire only on dispatch, in order, once")
    {
        Listener Events;
        Handler->OnDeviceConnected().connect<&Listener::OnConnected>(Events);
        Handler->OnDeviceDisconnected().connect<&Listener::OnDisconnected>(Events);

        const InputDeviceId A = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "a"));
        const InputDeviceId B = Virtual->CreateDevice(MakeInfo(InputDeviceType::Virtual, "b"));
        Virtual->DisconnectDevice(A);
        REQUIRE(Events.Order.empty());

        Handler->DispatchDeferredEvents();
        REQUIRE(Events.Connected.size() == 2);
        REQUIRE(Events.Connected[0] == A.Value);
        REQUIRE(Events.Connected[1] == B.Value);
        REQUIRE(Events.Disconnected.size() == 1);
        REQUIRE(Events.Disconnected[0] == A.Value);
        REQUIRE(Events.Order.size() == 3);
        REQUIRE(Events.Order[2] == A.Value);

        Handler->DispatchDeferredEvents();
        REQUIRE(Events.Order.size() == 3);
    }
}

TEST_CASE("Input device kept after its handler is destroyed", "[Input]")
{
    auto Handler = InputMessageHandler::Create();
    auto Virtual = std::make_shared<VirtualInputDeviceInterface>();
    Handler->AddInterface(Virtual);
    const InputDeviceId Id = Virtual->CreateDevice(MakeInfo(InputDeviceType::Keyboard, "kb"));
    std::shared_ptr<InputDevice> Device = Handler->GetDeviceFactory().Find(Id);
    REQUIRE(Device != nullptr);

    Handler.reset();

    // The device is still alive, but it has no handler. Nothing may crash.
    Device->PushKey(Keys::W, true);
    Device->PushRepeat(Keys::W);
    Device->PushChar(65);
    Device->PushAxis(Keys::GamepadLeftX, 0.5f);
    Device->PushMouseMove(1, 2, 3.0f, 4.0f);
    Device->PushWheel(0.0f, 1.0f);
    Device->PushFocus(true);

    const InputKeyState State = Device->GetLatestKeyState(Keys::W);
    REQUIRE_FALSE(State.IsHeld);
    REQUIRE(State.PressCount == 0);
    REQUIRE(State.ReleaseCount == 0);
}
