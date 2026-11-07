#include "Systems/RotatorSystem.h"
#include "TransformUtil.h"

namespace Sandbox
{
    void RotatorSystem::FixedUpdate(Context& ctx) const
    {
        if (!ctx.Singleton<const SandboxToggles>().Rotate)
        {
            return;
        }

        const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;
        for (auto [entity, transform, rotator] : ctx.Query<Fling::Transform, const Rotator>())
        {
            Fling::TransformUtil::RotateAround(transform, glm::vec3(0.0f, 1.0f, 0.0f), 15.0f * dt);
        }
    }
} // namespace Sandbox
