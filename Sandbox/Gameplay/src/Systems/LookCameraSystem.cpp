#include "LookUtil.h"
#include "Systems/LookCameraSystem.h"

namespace Sandbox
{
    void LookCameraSystem::FixedUpdate(Context& ctx) const
    {
        const Fling::InputSlice& input = ctx.Singleton<const Fling::InputState>().Tick.Slice;
        for (auto [entity, camera] : ctx.Query<LookCamera>())
        {
            camera = LookUtil::Run(camera, input);
        }
    }
} // namespace Sandbox
