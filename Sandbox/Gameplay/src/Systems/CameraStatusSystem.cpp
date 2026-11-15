#include "Systems/CameraStatusSystem.h"

namespace Sandbox
{
    void CameraStatusSystem::Update(Context& ctx) const
    {
        CameraStatus& status = ctx.Singleton<CameraStatus>();
        status = CameraStatus{};
        for (auto [entity, camera] : ctx.Query<const LookCamera>())
        {
            const Stasis* stasis = ctx.TryGet<const Stasis>(entity);
            if (camera.Policy.InStasis && stasis != nullptr)
            {
                status.Mode = CameraMode::Stasis;
                status.StasisSecondsLeft = static_cast<float>(static_cast<double>(stasis->RemainingNs) * 1e-9);
            }
            else
            {
                status.Mode = camera.Policy.Frozen ? CameraMode::Frozen : CameraMode::Free;
            }
        }
    }
} // namespace Sandbox
