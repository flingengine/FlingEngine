#pragma once

// The look camera test rig: one input stream through the real queue, a fixed tick that
// consumes it (the runner writes InputState, Sandbox.LookCamera reads it), and a frame side
// that shows the input the tick has not consumed yet (the runner writes PendingInput,
// Sandbox.LookCameraView reads it). Input goes through a VirtualInputDeviceInterface on an
// InputMessageHandler, the engine's TickInputProvider for the ticks, and PendingInputReader
// for the frames, all on a virtual clock. LookCameraTests.cpp and StasisTests.cpp use it.

#include "Components/CameraView.h"
#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/Publish/PublishedSet.h"
#include "Ecs/SimTime.h"
#include "Ecs/SystemRegistry.h"
#include "Ecs/SystemRunner.h"
#include "Input/InputMessageHandler.h"
#include "Input/PendingInputReader.h"
#include "Input/TickInput.h"
#include "Input/TickInputProvider.h"
#include "Input/VirtualInputDeviceInterface.h"
#include "LookCamera.h"
#include "LookUtil.h"
#include "PlatformTime.h"
#include "PresentationLink.h"
#include "SimRate.h"
#include "Stasis.h"
#include "Systems/LookCameraSystem.h"
#include "Systems/LookCameraViewSystem.h"

#include <bit>
#include <catch2/catch_all.hpp>
#include <functional>
#include <memory>
#include <span>
#include <vector>

namespace SandboxTest
{
    using namespace Fling;

    /** Bit-exact float compare: the two sides must agree to the last bit, not only nearly. */
    inline bool SameBits(const float a, const float b)
    {
        return std::bit_cast<uint32>(a) == std::bit_cast<uint32>(b);
    }

    inline bool SameBits(const LookView& a, const LookView& b)
    {
        return SameBits(a.Yaw, b.Yaw) && SameBits(a.Pitch, b.Pitch);
    }

    /** One message of the input stream as the rig pushed it: a mouse move or a key edge. */
    struct StreamEvent
    {
        bool Mouse = true;
        int32 Dx = 0;
        int32 Dy = 0;
        InputKey Key;
        bool Down = false;
    };

    /**
     * The look camera's rule applied to a list of events in stream order, from a camera.
     * The sequences are made up (1, 2, ...): LookUtil::Run only uses their order.
     */
    inline LookCamera Reference(const LookCamera& t_Start, const std::span<const StreamEvent> t_Events)
    {
        InputSlice Slice;
        uint64 Sequence = 0;
        for (const StreamEvent& Event : t_Events)
        {
            ++Sequence;
            if (Event.Mouse)
            {
                Slice.MouseSamples.push_back(InputMouseSample{Sequence, Event.Dx, Event.Dy});
            }
            else
            {
                Slice.KeyEvents.push_back(InputKeyEvent{Sequence, Event.Key, Event.Down});
            }
        }
        return LookUtil::Run(t_Start, Slice);
    }

    inline bool SameBits(const LookCamera& a, const LookCamera& b)
    {
        return SameBits(a.View, b.View) && SameBits(a.Policy.LookScale, b.Policy.LookScale) &&
               a.Policy.Frozen == b.Policy.Frozen && a.Policy.InStasis == b.Policy.InStasis;
    }

    /** How a LookRig runs. The defaults are the rig the look camera tests use. */
    struct LookRigConfig
    {
        /** The tick rate: SimTime::FixedDeltaNs of every tick. */
        uint32 SimHz = SimRate::DefaultHz;

        /** Make the camera entity with a Stasis, as the Sandbox does. */
        bool WithStasis = false;

        /**
         * The frame side reads a mirror (MirrorLink), as in Inline and Threaded play: a tick
         * reaches the frames only when it is published. Tick publishes at once; TickUnpublished
         * and Publish let a test publish late, as a busy game thread does.
         */
        bool Mirror = false;
    };

    /**
     * An input handler with a virtual mouse and keyboard, the engine input systems and
     * the look camera systems, and one LookCamera entity. Push moves and presses "from
     * the input thread" with Move and Press (they are in the queue at once), or queue a
     * move for the next Poll with QueueMove. Tick and Frame run the runner the way
     * World::Update does. t_Extra adds more systems (a cutscene, Sandbox.Stasis).
     */
    struct LookRig
    {
        PlatformTime::ScopedVirtualClock Clock{0};

        std::shared_ptr<InputMessageHandler> Handler = InputMessageHandler::Create();
        std::shared_ptr<VirtualInputDeviceInterface> Virtual;
        InputDeviceId Mouse;
        InputDeviceId Keyboard;

        EntityStorage Storage;
        SystemRegistry Systems;
        std::unique_ptr<SystemRunner> Runner;
        Entity Camera;

        /** Every event pushed, in stream order, and how many of them the ticks have consumed. */
        std::vector<StreamEvent> Stream;
        size_t Consumed = 0;
        LookCamera StartCamera;

        /** Times of the pushed messages, on the rig's virtual clock. */
        int64 NowNs = 0;

        /** False until the first tick: before it there is no cut, and nothing is pending. */
        bool Ticked = false;

        /** With LookRigConfig::Mirror: the packet path to the frame side's mirror. */
        std::unique_ptr<MirrorLink> Link;
        int64 FrameNs = 0;

        explicit LookRig(const LookCamera& t_Camera = LookCamera{},
                         const std::function<void(SystemRegistry&)>& t_Extra = {},
                         const LookRigConfig& t_Config = {})
        {
            const InputInterfaceId Id = Handler->AddInterface<VirtualInputDeviceInterface>();
            Virtual = Handler->FindInterface<VirtualInputDeviceInterface>(Id);

            InputDeviceInfo MouseInfo;
            MouseInfo.Type = InputDeviceType::Mouse;
            MouseInfo.HardwarePath = "virtual:mouse";
            MouseInfo.Capabilities = InputDeviceCapabilities::MouseButtons | InputDeviceCapabilities::RelativeMotion;
            Mouse = Virtual->CreateDevice(MouseInfo);

            InputDeviceInfo KeyboardInfo;
            KeyboardInfo.Type = InputDeviceType::Keyboard;
            KeyboardInfo.HardwarePath = "virtual:keyboard";
            KeyboardInfo.Capabilities = InputDeviceCapabilities::Keys;
            Keyboard = Virtual->CreateDevice(KeyboardInfo);
            Poll();

            Systems.Add<Sandbox::LookCameraSystem>();
            Systems.Add<Sandbox::LookCameraViewSystem>();
            if (t_Extra)
            {
                t_Extra(Systems);
            }
            ScheduleResult Schedule = Systems.BuildSchedule();
            REQUIRE(Schedule.ErrorReport() == "");
            Runner = std::make_unique<SystemRunner>(
                Systems, std::move(Schedule.Value), Storage, SystemRunnerConfig{.SimHz = t_Config.SimHz});

            // The engine's rule: Tick runs on request (RunTick), so it takes every queued message
            auto Inputs = std::make_shared<TickInputProvider>(Handler, nullptr, nullptr);
            Runner->SetTickInputSource([Inputs](TickInput& t_Out, const TickWindow& t_Window)
                                       { Inputs->Build(t_Out, t_Window); });
            auto Pending = std::make_shared<PendingInputReader>(Handler);
            Runner->SetPendingInputSource([Pending](const uint64 t_After, InputSlice& t_Out)
                                          { Pending->Read(t_After, t_Out); });
            Runner->Create();

            Camera = t_Config.WithStasis ? Storage.Create(t_Camera, Stasis{}) : Storage.Create(t_Camera);
            StartCamera = t_Camera;

            if (t_Config.Mirror)
            {
                PublishedSet Published;
                Published.Add<LookCamera>().Add<Stasis>();
                Link = std::make_unique<MirrorLink>(Published);
                Runner->SetFrameStorage(Link->FrameStorage());
                Publish();
            }
        }

        ~LookRig()
        {
            if (Link)
            {
                Runner->SetFrameStorage(Storage);
            }
        }

        LookRig(const LookRig&) = delete;
        LookRig& operator=(const LookRig&) = delete;

        void Poll()
        {
            NowNs += 1000;
            Clock.Clock().Set(NowNs);
            Handler->PollInterfaces(NowNs);
        }

        /** Queue a mouse move for the next Poll. */
        void QueueMove(const int32 t_Dx, const int32 t_Dy)
        {
            InputMessage Message;
            Message.Device = Mouse;
            Message.Type = InputMessageType::MouseMove;
            Message.DeltaX = t_Dx;
            Message.DeltaY = t_Dy;
            Virtual->Queue(Message);
            Stream.push_back(StreamEvent{.Mouse = true, .Dx = t_Dx, .Dy = t_Dy});
        }

        /** Queue a key press and release for the next Poll. */
        void QueuePress(const InputKey t_Key)
        {
            InputMessage Message;
            Message.Device = Keyboard;
            Message.Key = t_Key;
            for (const bool Down : {true, false})
            {
                Message.Type = Down ? InputMessageType::KeyDown : InputMessageType::KeyUp;
                Virtual->Queue(Message);
                Stream.push_back(StreamEvent{.Mouse = false, .Key = t_Key, .Down = Down});
            }
        }

        /** A mouse move that is in the queue at once (an input thread). */
        void Move(const int32 t_Dx, const int32 t_Dy)
        {
            QueueMove(t_Dx, t_Dy);
            Poll();
        }

        /** A key press and release, in the queue at once. */
        void Press(const InputKey t_Key)
        {
            QueuePress(t_Key);
            Poll();
        }

        void Tick()
        {
            TickUnpublished();
            Publish();
        }

        /** A tick that takes its cut now, and is not published to the mirror yet. As Tick without a mirror. */
        void TickUnpublished()
        {
            Runner->RunTick(WorldRole::Play);
            Consumed = Stream.size();
            Ticked = true;
        }

        /** With a mirror: publish the newest tick, for the next Frame. */
        void Publish()
        {
            if (Link)
            {
                Link->Publish(Storage, *Runner);
            }
        }

        void Frame()
        {
            if (!Link)
            {
                Runner->RunFrame(std::chrono::milliseconds(4), WorldRole::Play);
                return;
            }
            FrameNs += 4'000'000;
            const ShownTicks Latest = Link->ShowLatest();
            Runner->RunFrameAt(FrameNs, std::chrono::milliseconds(4), WorldRole::Play, Latest.Latest);
        }

        /** The storage the frame systems ran on: the simulation storage, or the mirror. */
        const EntityStorage& FrameStorage() const { return Link ? Link->FrameStorage() : Storage; }

        const LookCamera& Sim() const { return *Storage.TryGet<LookCamera>(Camera); }

        LookCamera& SimMut() { return *Storage.TryGet<LookCamera>(Camera); }

        /** The view the last frame drew (the CameraView that Sandbox.LookCameraView wrote). */
        LookView View() const
        {
            const CameraView& Shown = FrameStorage().Singleton<CameraView>();
            REQUIRE(Shown.Active);
            return LookView{Shown.Yaw, Shown.Pitch};
        }

        /** The stamp of the tick the last frame showed. */
        const TickStamp& Shown() const { return FrameStorage().Singleton<RealFrameTime>().Latest; }

        /** The mouse samples the last frame had pending. */
        size_t PendingSamples() const { return FrameStorage().Singleton<PendingInput>().MouseSamples.size(); }

        /** The events pushed after the last tick, as the rig counted them. */
        std::vector<StreamEvent> NotConsumed() const
        {
            return std::vector<StreamEvent>(Stream.begin() + static_cast<std::ptrdiff_t>(Consumed), Stream.end());
        }

        /**
         * The view after the last frame must be the sim camera run over exactly the input
         * after the last tick, mouse moves and key edges in their order.
         */
        void CheckFrame() const
        {
            const std::vector<StreamEvent> Expected = Ticked ? NotConsumed() : std::vector<StreamEvent>{};
            const PendingInput& Pending = FrameStorage().Singleton<PendingInput>();
            size_t Mouse = 0;
            size_t Key = 0;
            for (const StreamEvent& Event : Expected)
            {
                if (Event.Mouse)
                {
                    REQUIRE(Mouse < Pending.MouseSamples.size());
                    REQUIRE(Pending.MouseSamples[Mouse].DeltaX == Event.Dx);
                    REQUIRE(Pending.MouseSamples[Mouse].DeltaY == Event.Dy);
                    ++Mouse;
                }
                else
                {
                    REQUIRE(Key < Pending.KeyEvents.size());
                    REQUIRE(Pending.KeyEvents[Key].Key == Event.Key);
                    REQUIRE(Pending.KeyEvents[Key].Down == Event.Down);
                    ++Key;
                }
            }
            REQUIRE(Pending.MouseSamples.size() == Mouse);
            REQUIRE(Pending.KeyEvents.size() == Key);
            REQUIRE(Pending.Cut.AfterSequence == Shown().Cut.LastSequence);
            REQUIRE(SameBits(View(), Reference(Sim(), Expected).View));
        }

        /** The sim camera after a tick is every event pushed so far, run from the start. */
        void CheckSimHasAll() const
        {
            REQUIRE(Consumed == Stream.size());
            REQUIRE(SameBits(Sim(), Reference(StartCamera, Stream)));
        }
    };

    inline LookCamera MakeCamera(const float t_Scale, const float t_Pitch = 0.0f)
    {
        LookCamera Camera;
        Camera.Policy.LookScale = t_Scale;
        Camera.View.Pitch = t_Pitch;
        return Camera;
    }
} // namespace SandboxTest
