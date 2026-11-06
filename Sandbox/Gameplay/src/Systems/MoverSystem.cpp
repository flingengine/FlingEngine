#include "Systems/MoverSystem.h"

namespace Sandbox
{
    void MoverSystem::FixedUpdate(Context& ctx) const
    {
        if (!ctx.Singleton<const SandboxToggles>().MoveLights)
        {
            return;
        }

        const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;

        for (auto [entity, transform, mover] : ctx.Query<Fling::Transform, Mover>())
        {
            const glm::vec3 pos = transform.GetPos();

            // Turn around near the target
            if (glm::length(pos - mover.TargetPos) <= 0.5f)
            {
                mover.TargetPos = -1.0f * mover.TargetPos;
            }

            transform.SetPos(glm::lerp(pos, mover.TargetPos, mover.Speed * dt));
        }
    }
} // namespace Sandbox
