#pragma once

// A minimal look camera for the engine tests: one Simulation component that a fixed system
// turns with each tick's input, and one frame system that shows it with the input no tick
// has consumed yet. The engine tests use it as simulation state that depends on every input
// message and its order; it is not the Sandbox look camera (that one is tested in
// SandboxExampleTests).

#include "Components/CameraView.h"
#include "Components/InputState.h"
#include "Components/PendingInput.h"
#include "Ecs/Component.h"
#include "Ecs/ModuleRegistration.h"
#include "Ecs/SystemBase.h"
#include "Input/InputSlice.h"

#include <bit>

namespace TestLook
{
    /**
     * Turned by mouse samples, in radians per count. Keys change it at their place in the
     * input: F toggles Frozen, Equal doubles LookScale, Minus halves it. Rollback state.
     */
    struct TestCamera
    {
        static constexpr Fling::ComponentInfo Component{.Name = "TestCamera",
                                                        .Rollback = Fling::RollbackClass::Restore};

        float Yaw = 0.0f;
        float Pitch = 0.0f;
        float LookScale = 0.01f;
        bool Frozen = false;
    };

    /** The same bits, so a test can compare cameras made on two paths exactly. */
    inline bool SameBits(const TestCamera& t_A, const TestCamera& t_B)
    {
        return std::bit_cast<uint32>(t_A.Yaw) == std::bit_cast<uint32>(t_B.Yaw) &&
               std::bit_cast<uint32>(t_A.Pitch) == std::bit_cast<uint32>(t_B.Pitch) &&
               std::bit_cast<uint32>(t_A.LookScale) == std::bit_cast<uint32>(t_B.LookScale) && t_A.Frozen == t_B.Frozen;
    }

    /**
     * The camera after t_Input, walked in stream order. The tick and the frame run the same
     * rule, so Run(Run(c, a), b) == Run(c, a + b).
     */
    inline TestCamera Run(TestCamera t_Camera, const Fling::InputSlice& t_Input)
    {
        Fling::ForEachInOrder(
            t_Input.MouseSamples,
            t_Input.KeyEvents,
            [&](const Fling::InputMouseSample& t_Sample)
            {
                if (!t_Camera.Frozen)
                {
                    t_Camera.Yaw -= static_cast<float>(t_Sample.DeltaX) * t_Camera.LookScale;
                    t_Camera.Pitch -= static_cast<float>(t_Sample.DeltaY) * t_Camera.LookScale;
                }
            },
            [&](const Fling::InputKeyEvent& t_Key)
            {
                if (!t_Key.Down)
                {
                    return;
                }
                if (t_Key.Key == Fling::Keys::F)
                {
                    t_Camera.Frozen = !t_Camera.Frozen;
                }
                else if (t_Key.Key == Fling::Keys::Equal)
                {
                    t_Camera.LookScale *= 2.0f;
                }
                else if (t_Key.Key == Fling::Keys::Minus)
                {
                    t_Camera.LookScale *= 0.5f;
                }
            });
        return t_Camera;
    }

    /** Registers TestCamera with every field, so the state hash reads all of it. */
    inline void RegisterCameraType(Fling::ModuleRegistration& t_Modules)
    {
        t_Modules.Component<TestCamera>(
            [](Fling::TypeBuilder<TestCamera>& t_Type)
            {
                t_Type.Field("yaw", &TestCamera::Yaw);
                t_Type.Field("pitch", &TestCamera::Pitch);
                t_Type.Field("lookScale", &TestCamera::LookScale);
                t_Type.Field("frozen", &TestCamera::Frozen);
            });
    }

    /** The fixed system: runs each camera over this tick's input. */
    class CameraSystem final
        : public Fling::System<CameraSystem, Fling::Access<Fling::Read<Fling::InputState>, Fling::Write<TestCamera>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Test.Camera", .In = Fling::Order::Simulation};

        void FixedUpdate(Context& ctx) const
        {
            const Fling::InputSlice& Input = ctx.Singleton<const Fling::InputState>().Tick.Slice;
            for (auto [Entity, Camera] : ctx.Query<TestCamera>())
            {
                (void)Entity;
                Camera = Run(Camera, Input);
            }
        }
    };

    /** The frame system: the last tick's camera run over the pending input, written to CameraView. */
    class CameraViewSystem final
        : public Fling::System<
              CameraViewSystem,
              Fling::Access<Fling::Read<TestCamera>, Fling::Read<Fling::PendingInput>, Fling::Write<Fling::CameraView>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Test.CameraView", .In = Fling::Order::Presentation};

        void Update(Context& ctx) const
        {
            const Fling::PendingInput& Pending = ctx.Singleton<const Fling::PendingInput>();
            Fling::CameraView& View = ctx.Singleton<Fling::CameraView>();
            View.Active = false;
            for (auto [Entity, Camera] : ctx.Query<const TestCamera>())
            {
                (void)Entity;
                const TestCamera Shown = Run(Camera, Pending);
                View = Fling::CameraView{.Active = true, .Yaw = Shown.Yaw, .Pitch = Shown.Pitch};
            }
        }
    };
} // namespace TestLook
