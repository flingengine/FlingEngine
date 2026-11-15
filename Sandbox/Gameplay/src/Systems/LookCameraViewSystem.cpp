#include "LookUtil.h"
#include "Systems/LookCameraViewSystem.h"

namespace Sandbox
{
    void LookCameraViewSystem::Update(Context& ctx) const
    {
        const Fling::PendingInput& pending = ctx.Singleton<const Fling::PendingInput>();
        Fling::CameraView& view = ctx.Singleton<Fling::CameraView>();
        view.Active = false;
        for (auto [entity, camera] : ctx.Query<const LookCamera>())
        {
            const LookView shown = LookUtil::Run(camera, pending).View; // the same rule the tick runs
            view = Fling::CameraView{.Active = true, .Yaw = shown.Yaw, .Pitch = shown.Pitch};
        }
    }
} // namespace Sandbox
