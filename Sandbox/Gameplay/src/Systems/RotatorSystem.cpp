#include "Systems/RotatorSystem.h"

namespace Sandbox
{
    void RotatorSystem::FixedUpdate(Context& ctx) const
    {
        if (!ctx.Singleton<const SandboxToggles>().Rotate)
        {
            return;
        }

        const float dt = ctx.Singleton<const Fling::SimTime>().FixedDelta;
        const glm::vec3 offset(0.0f, 15.0f * dt, 0.0f);

        for (auto [entity, transform, rotator] : ctx.Query<Fling::Transform, const Rotator>())
        {
            transform.SetRotation(transform.GetRotation() + offset);
        }
    }
} // namespace Sandbox
