#pragma once

#include "Components/CameraView.h"
#include "Components/PendingInput.h"
#include "Ecs/SystemBase.h"
#include "LookCamera.h"

namespace Sandbox
{
    /**
     * Writes the CameraView from scratch every frame: the LookCamera that the last tick
     * made, run over the input that no tick has consumed yet (PendingInput) with the same
     * LookUtil::Run, key presses included. So it shows a freeze or a scale change at the
     * press, as the next tick will make it. It keeps no state, so input is never counted
     * twice: when a tick consumes it, it is in the LookCamera and no longer pending. It
     * reads nothing else from the simulation, so a policy change that does not come from
     * input (a cutscene) is shown from the first frame after that tick.
     *
     * The Sandbox has one LookCamera (Game::OnStartGame makes it). With none (the editor
     * before Play) the view is not active, and the renderer keeps its debug camera.
     */
    class LookCameraViewSystem final
        : public Fling::System<
              LookCameraViewSystem,
              Fling::Access<Fling::Read<LookCamera>, Fling::Read<Fling::PendingInput>, Fling::Write<Fling::CameraView>>>
    {
    public:
        static constexpr Fling::SystemInfo Info{.Name = "Sandbox.LookCameraView", .In = Fling::Order::Presentation};

        void Update(Context& ctx) const;
    };
} // namespace Sandbox
